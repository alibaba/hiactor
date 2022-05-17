/** Copyright 2021 Alibaba Group Holding Limited. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hiactor/core/root_actor_group.hh>
#include <hiactor/net/connection.hh>
#include <hiactor/net/network_config.hh>
#include <hiactor/net/network_io.hh>

namespace hiactor {

using tmp_buffer_t = seastar::temporary_buffer<char>;

struct network_delegater {
    static actor_message* deserialize(const tmp_buffer_t& buf) {
        // header | num_payload | ... | ... | ...
        static constexpr uint32_t offset = sizeof(actor_message::header) + 4;
        auto* ptr = buf.get();
        auto& am_hdr = *reinterpret_cast<const actor_message::header*>(ptr);
        auto nr_payload = *reinterpret_cast<const uint32_t*>(ptr + sizeof(actor_message::header));
        serializable_queue qu;
        ptr += offset;
        for (uint32_t i = 0; i < nr_payload; ++i) {
            auto len = *reinterpret_cast<const uint32_t*>(ptr);
            qu.push(tmp_buffer_t{ptr + 4, len});
            ptr += len + 4;
        }

        return new actor_message_with_payload<serializable_queue>{
            am_hdr.addr, am_hdr.behavior_tid, std::move(qu),
            am_hdr.src_shard_id, am_hdr.pr_id, am_hdr.m_type};
    }
};

void connection_manager::buffers_helper::abort(const std::exception_ptr& ex, std::vector<uint32_t>&& mids) {
    for (auto& id : mids) {
        // delete all messages to avoid memory leak.
        _qs[id].consume([](actor_message* msg) {
            delete msg;
            return true;
        });
        _qs[id].abort(ex);
    }
}

void connection_manager::buffers_helper::build() {
    _nr_bufs = network_config::get().num_machines();
    _qs = reinterpret_cast<net_buffer*>(operator new[] (sizeof(net_buffer) * _nr_bufs));
    for (uint32_t i = 0; i < _nr_bufs; ++i) {
        new (&_qs[i]) net_buffer(_qu_size);
    }
}

seastar::future<> connection_manager::server_side::start(connection_table& ct) {
    return optional_listen().then([this, &ct] {
        return collect_machine_ids(ct);
    });
}

seastar::future<> connection_manager::server_side::optional_listen() {
    if (init_ntc == 0) { return seastar::make_ready_future<>(); }
    auto listen_fut = seastar::repeat([this] {
        return this_listener->accept().then_wrapped([this](seastar::future<seastar::accept_result>&& fut) {
            try {
                auto res = fut.get0();
                _holder.emplace_back(0, connection{std::move(res.connection)});
                // Ensure atomic operation.
                auto cur_total_remains = num_total_conns.fetch_sub(1) - 1;
                if (cur_total_remains == 0) {
                    return seastar::parallel_for_each(boost::irange(0u, local_shard_count()), [this](unsigned c) {
                        if (local_shard_id() != c) {
                            return seastar::smp::submit_to(c, [lsn = listeners[c]] {
                                lsn->abort_accept();
                            });
                        } else {
                            return seastar::make_ready_future<>();
                        }
                    }).then_wrapped([](auto&& f) {
                        try { f.get(); } catch (...) {};
                        return seastar::make_ready_future<seastar::stop_iteration>(seastar::stop_iteration::yes);
                    });
                }
                return seastar::make_ready_future<seastar::stop_iteration>(seastar::stop_iteration::no);
            } catch (...) {
                return seastar::make_ready_future<seastar::stop_iteration>(seastar::stop_iteration::yes);
            }
        });
    });
    auto timeout_point = seastar::timer<>::clock::now() + std::chrono::seconds(network_config::get().timeout_seconds());
    return seastar::with_timeout(timeout_point, std::move(listen_fut)).then_wrapped([](seastar::future<> fut) {
        if (__builtin_expect(fut.failed(), false)) {
            fmt::print("[ Listening Timeout ] Server on shard {} misses connections from clients.\n", local_shard_id());
            return seastar::make_exception_future<>(fut.get_exception());
        }
        return seastar::make_ready_future<>();
    });
}

seastar::future<> connection_manager::server_side::collect_machine_ids(connection_table& conn_table) {
    return seastar::parallel_for_each(boost::irange(size_t(0), _holder.size()), [this](size_t pos) {
        auto& in = _holder[pos].second.in;
        return in.read_exactly(4).then([this, pos](tmp_buffer_t buf) mutable {
            auto peer_mid = *reinterpret_cast<const uint32_t*>(buf.get());
            // assign to real peer machine id
            _holder[pos].first = peer_mid;
        });
    }).then([this, &conn_table]() mutable {
        for (auto&& mc_pair : _holder) {
            conn_table.add(std::move(mc_pair));
        }
        // reclaim _holder heap memory.
        auto tmp_vec = std::move(_holder);
    });
}

seastar::future<> connection_manager::clients_side::start(connection_table& conn_table) {
    auto& server_list = network_config::get().get_server_list(local_shard_id());
    auto num_machs = static_cast<uint32_t>(server_list.size());
    return seastar::parallel_for_each(boost::irange(0u, num_machs),[&conn_table, &server_list](unsigned id) mutable {
        auto timeout_point = seastar::timer<>::clock::now() + std::chrono::seconds(network_config::get().timeout_seconds());
        return seastar::with_timeout(timeout_point, seastar::connect(server_list[id].first)
        ).then_wrapped([id, &conn_table](seastar::future<seastar::connected_socket> fut) {
            auto& sv_list = network_config::get().get_server_list(local_shard_id());
            uint32_t target_mid = sv_list[id].second;
            try {
                conn_table.add(std::make_pair(target_mid, connection{fut.get0()}));
                auto& out = conn_table.find(target_mid).out;
                auto this_mid = network_config::get().machine_id();
                return out.write(reinterpret_cast<const char*>(&this_mid), 4).then([&out] {
                    return out.flush();
                });
            } catch (...) {
                fmt::print("[ Connection Failed ] Cannot connect to server {} on machine {}.\n", sv_list[id].first, target_mid);
                return seastar::make_exception_future<>(std::current_exception());
            }
        });
    });
}

seastar::future<> connection_manager::run_loops(std::vector<uint32_t>& managed_mids) {
    managed_mids = _conn_table.managed_mach_ids();
    run_senders();
    run_receivers();
    return seastar::now();
}

seastar::future<> connection_manager::close_out_streams() {
    return seastar::parallel_for_each(_conn_table.mc_map() | boost::adaptors::map_values, [](connection& conn) {
        return conn.out.close();
    });
}

void connection_manager::run_senders() {
    _sender_stopped = seastar::parallel_for_each(_conn_table.managed_mach_ids(), [this](unsigned mid) {
        auto& conn = _conn_table.find(mid);
        auto& buf = _buffers._qs[mid];
        return seastar::repeat([&conn, &buf]() {
            return buf.pop_eventually().then_wrapped([&conn](seastar::future<actor_message*>&& f) {
                if (__builtin_expect(f.failed(), false)) {
                    // there is only one type of exception for buffer
                    f.ignore_ready_future();
                    return seastar::make_ready_future<seastar::stop_iteration>(seastar::stop_iteration::yes);
                }
                auto msg = f.get0();
                return msg->serialize(conn.out).then([msg, &conn] {
                    delete msg;
                    return conn.out.flush();
                }).then([] {
                    return seastar::stop_iteration::no;
                });
            });
        });
    }).finally([this] {
        return close_out_streams();
    });
}

void connection_manager::run_receivers() {
    _receiver_stopped = seastar::parallel_for_each(_conn_table.mc_map() | boost::adaptors::map_values, [this](connection& conn) {
        return seastar::do_until([&conn, this] { return conn.in.eof() || _abort_connection; }, [&conn, this] {
            return conn.in.read_exactly(4).then([&conn, this](tmp_buffer_t buf) {
                if (__builtin_expect(conn.in.eof() || _abort_connection, false)) {
                    return seastar::now();
                }
                auto nr_bytes = *as_u32_ptr(buf.get_write());
                return conn.in.read_exactly(nr_bytes).then([](tmp_buffer_t&& buf) {
                    auto* msg = network_delegater::deserialize(buf);
                    return actor_engine().send(msg);
                });
            });
        }).finally([&conn] {
            return conn.in.close();
        });
    });
}

seastar::future<> connection_manager::stop() {
    _abort_connection = true;
    _buffers.abort(std::make_exception_ptr(std::runtime_error("connection is closed")),
                   _conn_table.managed_mach_ids());
    return _sender_stopped.handle_exception([](auto ex) {}).finally([this] {
        return _receiver_stopped.handle_exception([](auto ex) {}).finally([] {});
    });
}

} // namespace hiactor
