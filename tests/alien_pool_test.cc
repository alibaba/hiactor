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
#include <hiactor/core/alien_thread_pool.hh>
#include <hiactor/testing/test_case.hh>
#include <seastar/core/print.hh>
#include <seastar/core/alien.hh>
#include <seastar/core/aligned_buffer.hh>
#include <algorithm>

class my_task : public hiactor::alien_task {
public:
    unsigned shard_id;
    
    my_task(unsigned id) : hiactor::alien_task(), shard_id(id) {};

    void run() override {
        auto id = shard_id;
        seastar::alien::submit_to(id, [id] {
            auto this_id = std::this_thread::get_id();
            std::cout << "Seastar thread " << this_id << " executed task "<< id << "\n";
            return seastar::make_ready_future<uint>(id);
        });
    }
};

HIACTOR_TEST_CASE(alien_pool) {
    hiactor::alien_thread_pool* thread_pool = new hiactor::alien_thread_pool(4);
    for (unsigned id : boost::irange(0u, 4u)) {
        thread_pool->add_task(new my_task(id));
    }
    thread_pool->initialize_and_run();
    delete thread_pool;

    return seastar::make_ready_future<>();
}
