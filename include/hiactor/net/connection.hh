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

#pragma once

#include <hiactor/core/local_channel.hh>
#include <hiactor/core/shard-config.hh>
#include <hiactor/net/defs.hh>
#include <hiactor/net/network_config.hh>

#include <set>
#include <unordered_map>
#include <boost/range/adaptors.hpp>
#include <seastar/core/future-util.hh>
#include <seastar/core/iostream.hh>
#include <seastar/core/reactor.hh>
#include <seastar/net/api.hh>

namespace hiactor {

class connection_manager {
    struct connection {
        seastar::connected_socket socket;
        seastar::input_stream<char> in;
        seastar::output_stream<char> out;
        connection(seastar::connected_socket&& cs) : socket(std::move(cs)), in(socket.input()), out(socket.output()) {}
        connection(connection&&) noexcept = default;
        connection& operator=(connection&&) = default;
    };

    struct connection_table {
        connection_table() = default;
        void add(std::pair<uint32_t, connection>&& mc_pair) {
            auto mach_id = mc_pair.first;
            if (_mc_map.find(mach_id) == _mc_map.end()) {
                _managed_mach_ids.push_back(mc_pair.first);
            }
            _mc_map.insert(std::move(mc_pair));
        }
        connection& find(uint32_t mach_id) {
            return _mc_map.find(mach_id)->second;
        }
        std::vector<uint32_t> managed_mach_ids() {
            return _managed_mach_ids;
        }
        std::unordered_multimap<uint32_t, connection>& mc_map() {
            return _mc_map;
        }
    private:
        std::unordered_multimap<uint32_t, connection> _mc_map;
        std::vector<uint32_t> _managed_mach_ids;
    };

    struct server_side {
        std::vector<seastar::server_socket*> listeners;
        seastar::server_socket* this_listener;
        std::atomic<uint32_t>& num_total_conns;
        const uint32_t init_ntc;
        std::vector<std::pair<uint32_t, connection>> _holder;
        server_side(const std::vector<seastar::server_socket*>& lsns, std::atomic<uint32_t>& ntc, uint32_t i_ntc)
            : listeners(lsns), this_listener(listeners[local_shard_id()]), num_total_conns(ntc), init_ntc(i_ntc) {}
        ~server_side() { delete this_listener; }
        seastar::future<> start(connection_table& ct);
    private:
        seastar::future<> collect_machine_ids(connection_table& ct);
        seastar::future<> optional_listen();
    };

    struct clients_side {
        clients_side() = default;
        seastar::future<> start(connection_table& ct);
    };

    struct buffers_helper {
        net_buffer* _qs;
        uint32_t _nr_bufs;
        uint32_t _qu_size;
        explicit buffers_helper(uint32_t qu_size);
        ~buffers_helper();
        void build();
        void abort(const std::exception_ptr& ex,
                   std::vector<uint32_t>&& mids);
        net_buffer& get(uint32_t mach_id) {
            return _qs[mach_id];
        }
    };

public:
    connection_manager(const std::vector<seastar::server_socket*>& listeners,
                       std::atomic<uint32_t>& ntc, uint32_t init_ntc, const uint32_t q_size = 16)
        : _buffers(q_size), _as_server(listeners, ntc, init_ntc), _as_clients(), _server_ready(seastar::now()),
          _receiver_stopped(seastar::now()), _sender_stopped(seastar::now()), _abort_connection(false) {
        // FIXME: move to other where.
        _buffers.build();
    }

    seastar::future<> start_server() {
        _server_ready = _as_server.start(_conn_table);
        return seastar::now();
    }

    seastar::future<> establish_connections() {
        return seastar::when_all(_as_clients.start(_conn_table), std::move(_server_ready)).discard_result();
    }

    seastar::future<> run_loops(std::vector<uint32_t>& managed_mids);
    seastar::future<> stop();

private:
    seastar::future<> close_out_streams();
    void run_senders();
    void run_receivers();

private:
    buffers_helper _buffers;
    connection_table _conn_table;
    server_side _as_server;
    clients_side _as_clients;
    seastar::future<> _server_ready;
    seastar::future<> _receiver_stopped;
    seastar::future<> _sender_stopped;
    bool _abort_connection;

    friend class network_io;
    friend class network_channel;
};

inline
connection_manager::buffers_helper::buffers_helper(uint32_t qu_size)
    : _qs(nullptr), _nr_bufs(0), _qu_size(qu_size) {}

inline
connection_manager::buffers_helper::~buffers_helper() {
    for (uint32_t i = 0; i < _nr_bufs; ++i) {
        _qs[i].~net_buffer();
    }
    ::operator delete[](_qs);
}

} // namespace hiactor
