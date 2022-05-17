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

#include <hiactor/core/coordinator.hh>
#include <hiactor/core/thread_resource_pool.hh>
#include <hiactor/net/lba_policy.hh>
#include <hiactor/net/network_channel.hh>
#include <hiactor/net/network_io.hh>

using namespace std::chrono_literals;

namespace hiactor {

seastar::future<> network_io::start_and_connect() {
    // start() function only can called on shard 0.
    assert(local_shard_id() == 0);
    if (_running) { return seastar::make_ready_future<>(); }
    _running = true;
    return start_server().then([] {
        return seastar::parallel_for_each(boost::irange(0u, local_shard_count()), [](unsigned c) {
            return seastar::smp::submit_to(c, [] {
                return coordinator::get().global_barrier("HIACTOR_SERVER_STARTED");
            });
        });
    }).then([] {
        return establish_connections();
    }).then([] {
        return seastar::parallel_for_each(boost::irange(0u, local_shard_count()), [](unsigned c) {
            return seastar::smp::submit_to(c, [] {
                return coordinator::get().global_barrier("HIACTOR_CONNECTION_ESTABLISHED");
            });
        });
    });
}

seastar::future<> network_io::start_server() {
    _cms = cms{construct_entries<cm>(), entries_deleter<cm>{local_shard_count()}};
    // Register cleanup task to engine.
    seastar::engine().at_exit([] { return stop(); });
    seastar::net::customized_lba::set_policy(
        std::unique_ptr<seastar::net::lba_policy>(new hiactor_lba_policy()));

    auto& nc_instance = network_config::get();
    network_io::_listeners.resize(local_shard_count());
    network_io::_ntc.store(nc_instance.waiting_conn_count());

    return seastar::parallel_for_each(
        boost::irange(0u, local_shard_count()), [port = nc_instance.listen_port()](unsigned c) {
            return seastar::smp::submit_to(c, [port] {
                seastar::listen_options lo;
                lo.reuse_address = true;
                lo.lba = seastar::server_socket::load_balancing_algorithm::customized;
                auto* this_listener = new seastar::server_socket{
                    seastar::engine().listen(seastar::make_ipv4_address({port}), lo)};
                network_io::_listeners[local_shard_id()] = this_listener;
            });
        }).then([init_ntc = nc_instance.waiting_conn_count()] {
        return seastar::parallel_for_each(boost::irange(0u, local_shard_count()), [init_ntc](unsigned c) {
            return seastar::smp::submit_to(c, [init_ntc] {
                // construct and then start it on the target shard.
                _cms[local_shard_id()].init(network_io::_listeners, network_io::_ntc, init_ntc);
                return get_local_cm().start_server();
            });
        });
    });
}

seastar::future<> network_io::establish_connections() {
    using mid_container = std::vector<std::vector<uint32_t>>;
    return seastar::do_with(mid_container(local_shard_count()), [](mid_container& sharded_mids) mutable {
        return seastar::parallel_for_each(boost::irange(0u, local_shard_count()), [](unsigned c) {
            return seastar::smp::submit_to(c, [] { return get_local_cm().establish_connections(); });
        }).then([&sharded_mids]() mutable {
            return seastar::parallel_for_each(boost::irange(0u, local_shard_count()), [&sharded_mids](unsigned c) mutable {
                return seastar::smp::submit_to(c, [&sharded_mids, c]() mutable {
                    return get_local_cm().run_loops(sharded_mids[c]);
                });
            });
        }).then([&sharded_mids]() mutable {
            network_channel::_routing_tbl.build(sharded_mids);
            network_channel::_cms = _cms.get();
        });
    });
}

seastar::future<> network_io::stop() {
    return seastar::parallel_for_each(boost::irange(0u, local_shard_count()), [](unsigned c) {
        return seastar::smp::submit_to(c, [] {
            if (!_cms[local_shard_id()].empty()) {
                // stop and then destruct it on the target shard.
                return get_local_cm().stop().then([] {
                    _cms[local_shard_id()].finalize();
                });
            }
            // otherwise...
            return seastar::make_ready_future<>();
        });
    });
}

network_io::cms network_io::_cms;
bool network_io::_running = false;
std::atomic<uint32_t> network_io::_ntc(0);
std::vector<seastar::server_socket*> network_io::_listeners;

} // namespace hiactor
