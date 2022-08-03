/** Copyright 2022 Alibaba Group Holding Limited. All Rights Reserved.
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
#include <hiactor/core/actor-template.hh>
#include <hiactor/core/reference_base.hh>
#include <hiactor/util/data_type.hh>
#include <seastar/core/future-util.hh>
#include <seastar/core/print.hh>
#include <seastar/core/semaphore.hh>
#include <seastar/core/sleep.hh>
#include <seastar/core/thread.hh>

class count_ref : public hiactor::reference_base {
public:
    count_ref();
    seastar::future<hiactor::Integer> check();
    seastar::future<hiactor::Integer> increase(hiactor::Integer &&amount);
};

class ANNOTATION(actor : impl) count : public hiactor::actor {
    int count_res = 0;

public:
    count(hiactor::actor_base *exec_ctx, const hiactor::byte_t *addr) : hiactor::actor(exec_ctx, addr, false) {}
    ~count() override = default;

    /// increase count by `amount`, return the remaining count after increase,
    seastar::future<hiactor::Integer> ANNOTATION(actor : method) increase(hiactor::Integer &&amount);

    /// Check info and return the res.
    seastar::future<hiactor::Integer> ANNOTATION(actor : method) check();

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};
