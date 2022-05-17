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

#include <pthread.h>
#include <cassert>
#include <iostream>
#include <queue>

namespace hiactor {

class alien_task {
public:
    alien_task() = default;
    virtual ~alien_task() = default;
    virtual void run() = 0;
};

class task_queue {
    std::queue<alien_task*> _tasks;
    pthread_mutex_t _qmtx{};
public:
    task_queue();
    ~task_queue();

    alien_task* next_task();
    void add_task(alien_task* nt);
};

class alien_thread_pool {
    task_queue _queue;
    const uint32_t _num_threads;
    std::vector<pthread_t> _threads;
public:
    explicit alien_thread_pool(uint32_t num_threads);
    ~alien_thread_pool();

    // Allocate a thread pool and set them to work trying to get tasks
    void initialize_and_run();

    void add_task(alien_task* t) {
        _queue.add_task(t);
    }
};

} // namespace hiactor
