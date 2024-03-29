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
#include <hiactor/core/thread_resource_pool.hh>
#include <seastar/core/print.hh>

using namespace std::chrono_literals;

int main(int argc, char** argv) {
    hiactor::actor_app app;
    app.run(argc, argv, [] {
        return seastar::parallel_for_each(boost::irange<unsigned>(0u, hiactor::local_shard_count()), [] (unsigned id) {
            return seastar::smp::submit_to(id, [id] {
                return hiactor::thread_resource_pool::submit_work([id] {
                    fmt::print("I'm working in resource thread! from reactor {} \n", id);
                    return id * id;
                }).then_wrapped([id] (seastar::future<unsigned> fut) {
                    try{
                        fmt::print("Shard {} get the result {} from resource thread.\n", id, fut.get0());
                    } catch (std::exception& ex) {
                        fmt::print("Exception: {}\n", ex.what());
                    }
                });
            }).then([] {
                return seastar::make_ready_future<>();
            });
        }).then([] {
            fmt::print("Exit.\n");
            hiactor::actor_engine().exit();
        });
    });
}