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

#include <chrono>

namespace hiactor {

using clock_unit = std::chrono::microseconds;

const clock_unit default_actor_quota = clock_unit(500);
extern __thread std::chrono::steady_clock::time_point actor_clock;
extern __thread std::chrono::microseconds actor_quota;

extern uint32_t default_execution_interval;
extern __thread uint32_t actor_execution_count;

inline
std::chrono::steady_clock::time_point read_actor_clock() {
    return actor_clock;
}

inline
void advance_actor_clock() {
    if (++actor_execution_count < default_execution_interval) { return; }
    actor_execution_count = 0;
    actor_clock = std::chrono::steady_clock::now();
}

inline
clock_unit read_actor_quota() {
    return actor_quota;
}

inline
void set_actor_quota(clock_unit quota) {
    actor_quota = quota;
}

} // namespace hiactor
