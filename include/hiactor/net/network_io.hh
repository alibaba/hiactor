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

#include <hiactor/core/coordinator.hh>
#include <hiactor/core/shard-config.hh>
#include <hiactor/net/connection.hh>
#include <hiactor/net/defs.hh>
#include <hiactor/net/entry.hh>
#include <hiactor/net/network_config.hh>

#include <atomic>

namespace hiactor {

class connection_manager;

class network_io {
    using cm = entry<connection_manager>;
    using cms = std::unique_ptr<cm[], entries_deleter < cm>>;
public:
    static seastar::future<> start_and_connect();
private:
    static seastar::future<> start_server();
    static seastar::future<> establish_connections();
    static seastar::future<> stop();
    template <typename E>
    static E* construct_entries();

    static connection_manager& get_local_cm() {
        return _cms[local_shard_id()].o.t;
    }

    static cms _cms;
    static bool _running;
    static std::atomic<uint32_t> _ntc;
    static std::vector<seastar::server_socket*> _listeners;
};

template <typename E>
inline
E* network_io::construct_entries() {
    auto* ptr = reinterpret_cast<E*>(operator new[](sizeof(E) * local_shard_count()));
    for (unsigned i = 0; i < local_shard_count(); ++i) {
        new(&ptr[i]) E();
    }
    return ptr;
}

} // namespace hiactor
