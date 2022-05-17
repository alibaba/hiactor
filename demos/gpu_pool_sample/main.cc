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
#include <hiactor/core/gpu_resource_pool.hh>
#include <seastar/core/print.hh>
#include "cuda/cuda.h"

using namespace std::chrono_literals;

void variables_free(unsigned device_id, int *h_a, int *h_b, int *h_c, int *d_a, int *d_b, int *d_c) {
    delete[] h_a;
    delete[] h_b;
    delete[] h_c;

    cuda_set_device(device_id);
    d_free_space(d_a);
    d_free_space(d_b);
    d_free_space(d_c);
}

int main(int argc, char** argv) {
    unsigned task_num = 16;
    hiactor::actor_system app;
    app.run(argc, argv, [task_num] {
        return seastar::parallel_for_each(boost::irange(0u, task_num), [] (unsigned task_id) {
            unsigned id = task_id % hiactor::local_shard_count();
            return seastar::smp::submit_to(id, [task_id, id] {
                unsigned dev_id = task_id % cuda_get_device_count();
                cuda_set_device(dev_id);

                unsigned array_size = task_id + 1;
                int *h_a; int *h_b; int *h_c; int *d_a; int *d_b; int *d_c;
                h_a = new int[array_size];
                h_b = new int[array_size];
                h_c = new int[array_size];
                for (unsigned i = 0; i < array_size; ++i) {
                    h_a[i] = h_b[i] = i;
                }
                d_a = static_cast<int *>(d_alloc_space(sizeof(int) * array_size));
                d_b = static_cast<int *>(d_alloc_space(sizeof(int) * array_size));
                d_c = static_cast<int *>(d_alloc_space(sizeof(int) * array_size));
                return hiactor::gpu_resource_pool::submit_work(dev_id,
                    std::function<decltype(h_vec_add)>(h_vec_add), h_a, h_b, h_c, d_a, d_b, d_c, array_size
                ).then([task_id, id, dev_id, h_a, h_b, h_c, d_a, d_b, d_c, array_size] {
                    std::string results = "";
                    for (unsigned i = 0; i < array_size; ++i) {
                        results += std::to_string(h_c[i]) + "\t";
                    }
                    fmt::print("Get task {} result from gpu {} on shard {}: {}\n", task_id, dev_id, id, results);
                    variables_free(dev_id, h_a, h_b, h_c, d_a, d_b, d_c);
                });
            });
        }).then([] {
            seastar::fmt::print("Sleep...\n");
            return sleep(2s);
        }).then([] {
            fmt::print("Exit.\n");
            hiactor::actor_engine().exit();
        });
    });
}
