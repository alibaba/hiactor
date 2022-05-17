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

#include <hiactor/core/alien_thread_pool.hh>

namespace hiactor {

task_queue::task_queue() {
    pthread_mutex_init(&_qmtx, nullptr);
}

task_queue::~task_queue() {
    assert(_tasks.empty());
    pthread_mutex_destroy(&_qmtx);
}

alien_task* task_queue::next_task() {
    pthread_mutex_lock(&_qmtx);
    if (_tasks.empty()) {
        pthread_mutex_unlock(&_qmtx);
        return nullptr;
    }
    auto next = _tasks.front();
    _tasks.pop();
    pthread_mutex_unlock(&_qmtx);
    return next;
}

void task_queue::add_task(alien_task* nt) {
    pthread_mutex_lock(&_qmtx);
    _tasks.push(nt);
    pthread_mutex_unlock(&_qmtx);
}

void* thread_routine(void* arg) {
    auto* queue = (task_queue*) arg;
    alien_task* task = queue->next_task();
    while (task != nullptr) {
        task->run();
        delete task;
        task = queue->next_task();
    }
    pthread_exit(nullptr);
}

alien_thread_pool::alien_thread_pool(uint32_t num_threads)
    : _num_threads(num_threads), _threads(num_threads) {}

alien_thread_pool::~alien_thread_pool() {
    for (uint32_t i = 0; i < _num_threads; ++i) {
        pthread_join(_threads[i], nullptr);
    }
}

void alien_thread_pool::initialize_and_run() {
    for (uint32_t i = 0; i < _num_threads; ++i) {
        auto rc = pthread_create(&_threads[i], nullptr, thread_routine, (void*) (&_queue));
        if (rc) {
            std::cout << "Error:unable to create alien thread," << rc << std::endl;
            exit(-1);
        }
    }
}

} // namespace hiactor
