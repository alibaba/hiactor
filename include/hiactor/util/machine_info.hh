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

#include <cstdint>

namespace hiactor {

struct machine_info {
    /// global shard id anchor
    static uint32_t sid_anchor() {
        return _sid_anchor;
    }

    /// Number of shards in the cluster.
    static uint32_t num_shards() {
        return _num_shards;
    }

    static bool is_local(const uint32_t shard_id) {
        return shard_id >= _min_sid && shard_id <= _max_sid;
    }

private:
    static unsigned _num_shards;
    static unsigned _sid_anchor;
    static unsigned _min_sid;
    static unsigned _max_sid;

    friend class actor_app;
};

} // namespace hiactor
