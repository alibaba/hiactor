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

#include <hiactor/net/network_config.hh>

#include <seastar/net/api.hh>

namespace hiactor {

struct hiactor_lba_policy : public seastar::net::lba_policy {
    hiactor_lba_policy() = default;
    ~hiactor_lba_policy() override = default;
    unsigned get_cpu(uint32_t addr, uint16_t port) override {
        return network_config::get().get_client_target_shard(addr);
    }
};

} // namespace hiactor
