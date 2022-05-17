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

#include <hiactor/core/actor_timer.hh>

namespace hiactor {

__thread std::chrono::steady_clock::time_point actor_clock;
__thread std::chrono::microseconds actor_quota = default_actor_quota;

uint32_t default_execution_interval = 1;
__thread uint32_t actor_execution_count = 0;

} // namespace hiactor
