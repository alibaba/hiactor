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

#include <hiactor/core/actor_message.hh>
#include <hiactor/core/dynamic-queue.hh>

#include <cstdint>
#include <seastar/net/socket_defs.hh>

namespace hiactor {

struct worker_node_info {
    uint32_t machine_id;
    uint32_t nr_cores;
    seastar::socket_address addr;

    worker_node_info(uint32_t machine_id, uint32_t nr_cores, seastar::socket_address&& addr)
        : machine_id(machine_id), nr_cores(nr_cores), addr(std::move(addr)) {}
};

struct message_item {
    uint32_t mach_id;
    actor_message* msg;
    message_item() = default;
    message_item(uint32_t mid, actor_message* msg) : mach_id(mid), msg(msg) {}
};

using net_buffer = dynamic_queue<actor_message*>;

} // namespace hiactor
