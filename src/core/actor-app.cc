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

#include <hiactor/core/actor-app.hh>
#include <hiactor/core/actor_timer.hh>
#include <hiactor/core/coordinator.hh>
#include <hiactor/core/gpu_resource_pool.hh>
#include <hiactor/core/local_channel.hh>
#include <hiactor/core/thread_resource_pool.hh>
#include <hiactor/net/network_channel.hh>
#include <hiactor/net/network_io.hh>
#include <hiactor/util/machine_info.hh>

#include <fstream>

namespace hiactor {

namespace bpo = boost::program_options;

std::vector<root_actor_group*> actor_smp::_root_actor_groups;

seastar::future<> actor_smp::configure(const bpo::variables_map& vm) {
    if (vm.count("execution-interval")) {
      default_execution_interval = std::max(vm["execution-interval"].as<uint32_t>(), 1U);
    }
    assert(local_shard_id() == 0);
    _root_actor_groups.resize(local_shard_count());
    return seastar::smp::invoke_on_all([] {
        init_default_actor_msg_service_group();
        allocate_actor_engine();
        _root_actor_groups[local_shard_id()] = &actor_engine();
    }).then([&vm] {
        coordinator::get().launch_worker(&seastar::engine());
        local_channel::create_qs(_root_actor_groups);
        network_channel::create_qs(_root_actor_groups);
        thread_resource_pool::configure(vm);
#ifdef HIACTOR_GPU_ENABLE
        gpu_resource_pool::configure(vm);
#endif
        return seastar::smp::invoke_on_all([] {
            start_all_queues();
            actor_engine().register_actor_pollers();
        });
    });
}

void actor_smp::cleanup_cpu() {
    size_t shard_id = local_shard_id();
    if (local_channel::_qs) {
        for (unsigned i = 0; i < local_shard_count(); i++) {
            local_channel::_qs[i][shard_id].stop();
        }
    }
    if (network_channel::_qs) {
        for (unsigned i = 0; i < local_shard_count(); i++) {
            network_channel::_qs[i][shard_id].stop();
        }
    }
}

void actor_smp::start_all_queues() {
    for (unsigned c = 0; c < local_shard_count(); c++) {
        if (c != local_shard_id()) {
            local_channel::_qs[c][local_shard_id()].start(c);
            network_channel::_qs[c][local_shard_id()].start(c);
        }
    }
}

void actor_app::add_actor_options() {
    _app.add_options()
        ("execution-interval", bpo::value<uint32_t>(), "The number of messages/tasks an actor can process/execute before it checks if the quota of the current execution context is used up.")
        ("machine-id", bpo::value<uint32_t>(), "machine-id")
        ("worker-node-list", bpo::value<std::string>(), "worker node list file.")
        ("p2p-connection-count", bpo::value<uint32_t>()->default_value(1),
         "connection count between each two workers")
        ("network-timeout-sec", bpo::value<uint32_t>()->default_value(60),
         "timeout seconds of network connection.")
        ("open-thread-resource-pool", bpo::value<bool>()->default_value(false),
         "open thread resource pool.")
        ("worker-thread-number", bpo::value<uint32_t>()->default_value(8),
         "worker thread resource number.")
        ("thread-resource-retry-interval-us", bpo::value<int64_t>()->default_value(500),
         "thread resource re-acquire interval in us.")
#ifdef HIACTOR_GPU_ENABLE
        ("stream-num-per-gpu", bpo::value<uint32_t>()->default_value(4),
         "cuda stream resource number per gpu device.")
        ("max-task-num-per-stream", bpo::value<uint32_t>()->default_value(2),
         "max gpu tasks per stream.")
#endif
        ;
}

unsigned machine_info::_num_shards = 0;
unsigned machine_info::_sid_anchor = 0;
unsigned machine_info::_min_sid = 0;
unsigned machine_info::_max_sid = 0;

void load_worker_node_list(const std::string& fname, std::vector<worker_node_info>& configs, const uint32_t this_mid) {
    std::ifstream in(fname.c_str());
    std::stringstream ss;
    std::string line, ip;
    uint32_t mach = 0, nr_core = 0;
    // Ignore the first line.
    std::getline(in, line);
    while (in >> mach >> nr_core >> ip) {
        configs.emplace_back(mach, nr_core, seastar::make_ipv4_address(seastar::ipv4_addr(ip)));
    }

    if (this_mid >= configs.size()) {
        throw bpo::error("wrong machine_id (machine_id >= #machines) in `worker-node-list`.");
    }

    if (local_shard_count() != configs[this_mid].nr_cores) {
        throw bpo::error("incorrect number of cores for this machine in `worker-node-list`.");
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(configs.size()); ++i) {
        if (configs[i].machine_id != i) {
            throw bpo::error("machine ids must be range(0, #machines) in `worker-node-list`.");
        }
    }
}

void actor_app::set_network_config(uint32_t this_mid, uint32_t p2p_conn_count) {
    auto& nc_instance = network_config::get();
    auto& configs = nc_instance._node_list;
    auto this_mach_cores = local_shard_count();
    nc_instance._machine_id = this_mid;
    nc_instance._wait_conn_count = 0;
    nc_instance._p2p_conn_count = p2p_conn_count;
    nc_instance._listen_port = configs[this_mid].addr.port();
    nc_instance._num_machines = uint32_t((configs.size()));
    nc_instance._server_lists.resize(this_mach_cores);
    unsigned num_global_shards = 0;
    unsigned sid_anchor = 0;
    uint32_t dst_shard = 0;

    for (auto& conf : configs) {
        auto conf_mid = conf.machine_id;
        if (this_mid != conf_mid) {
            if (this_mid < conf_mid) {
                // As server.
                nc_instance._wait_conn_count += p2p_conn_count;
                auto ia = seastar::net::ntoh(conf.addr.as_posix_sockaddr_in().sin_addr.s_addr);
                // For pseudo-distributed mode
                if (nc_instance._addr_lsid_map.find(ia) == nc_instance._addr_lsid_map.end()) {
                    nc_instance._addr_lsid_map[ia] = dst_shard;
                }
                dst_shard = (dst_shard + p2p_conn_count) % this_mach_cores;
            } else {
                // Count shard id anchor for this machine.
                sid_anchor += conf.nr_cores;
                // As clients
                for (uint32_t i = 0; i < p2p_conn_count; ++i) {
                    nc_instance._server_lists[dst_shard].push_back(
                        std::make_pair(conf.addr, conf_mid));
                    dst_shard = (dst_shard + 1) % this_mach_cores;
                }
            }
        }
        num_global_shards += conf.nr_cores;
    }
    machine_info::_num_shards = num_global_shards;
    machine_info::_sid_anchor = sid_anchor;
}

seastar::future<> actor_app::start_network() {
    auto& configuration = _app.configuration();
    if (configuration.count("worker-node-list")) {
        if (!configuration.count("machine-id")) {
            auto ex = bpo::error(
                "command line argument `--machine-id` must be specified in distributed mode.");
            return seastar::make_exception_future<>(std::move(ex));
        }
        uint32_t machine_id = configuration["machine-id"].as<uint32_t>();
        auto list_file = configuration["worker-node-list"].as<std::string>();
        auto p2p_conn_count = configuration["p2p-connection-count"].as<uint32_t>();
        try {
            load_worker_node_list(list_file, network_config::get()._node_list, machine_id);
        } catch (...) {
            return seastar::make_exception_future<>(std::current_exception());
        }
        set_network_config(machine_id, p2p_conn_count);
        try {
            auto customized_timeout = configuration["network-timeout-sec"].as<uint32_t>();
            if (customized_timeout == 0) {
                throw bpo::error("Network connection timeout seconds cannot be set to 0.");
            }
            network_config::get()._timeout_seconds =
                std::min(customized_timeout, network_config::get()._timeout_limit);
        } catch (...) {
            return seastar::make_exception_future<>(std::current_exception());
        }
    } else {
        // non-distributed mode
        machine_info::_num_shards = local_shard_count();
        machine_info::_sid_anchor = 0;
    }
    machine_info::_min_sid = machine_info::sid_anchor();
    machine_info::_max_sid = machine_info::sid_anchor() + local_shard_count() - 1;
    coordinator::get().set_sync_size(machine_info::_num_shards);

    if (network_config::get().num_machines() > 1) {
        return network_io::start_and_connect();
    }
    return seastar::make_ready_future<>();
}

int actor_app::run(int ac, char** av, std::function<void()>&& func) {
    add_actor_options();
    return _app.run_deprecated(ac, av, [this, fn = std::move(func)]() mutable {
        return actor_smp::configure(_app.configuration()).then([this]() {
            return start_network();
        }).then([fn = std::move(fn)]() mutable {
            return seastar::futurize_invoke(std::move(fn));
        }).then_wrapped([](auto&& f) {
            try {
                f.get();
            } catch (bpo::error& e) {
                std::cerr << "Actor system failed with configuration exception: "
                          << e.what() << std::endl;
                actor_engine().exit();
            } catch (std::runtime_error& e) {
                std::cerr << "Actor system failed with runtime exception: "
                          << e.what() << std::endl;
                actor_engine().exit();
            } catch (seastar::timed_out_error& e) {
                std::cerr << "Actor system failed with network connection exception: "
                          << e.what() << std::endl;
                actor_engine().exit();
            } catch (...) {
                std::cerr << "Actor system failed with uncaught exception: "
                          << std::current_exception() << std::endl;
                actor_engine().exit();
            }
        });
    });
}

} // namespace hiactor
