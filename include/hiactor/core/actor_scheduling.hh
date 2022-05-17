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

#include <hiactor/core/actor_core.hh>

#include <algorithm>
#include <deque>
#include <seastar/core/circular_buffer.hh>
#include <seastar/core/task.hh>

namespace hiactor {

class schedulable_actor_group;

struct scheduling_comparator {
    const schedulable_actor_group* _group;

    explicit scheduling_comparator(const schedulable_actor_group* group) : _group(group) {}
    bool operator()(const actor_base* a, const actor_base* b) const;
};

/**
 * Schedulable task queue with state
 */
class schedulable_actor_task_queue {
    seastar::circular_buffer<actor_base*> _urgent_q{};
    seastar::circular_buffer<actor_base*> _q{};
    scheduling_comparator* _comparator = nullptr;
    bool _need_scheduling = false;

public:
    schedulable_actor_task_queue() = default;
    ~schedulable_actor_task_queue() {
        delete _comparator;
    }

    void set_comparator(scheduling_comparator* comparator) {
        delete _comparator;
        _comparator = comparator;
    }

    void push_urgent_task(actor_base* actor_task) {
        _urgent_q.push_back(actor_task);
    }

    void push_task(actor_base* actor_task) {
        _q.push_back(actor_task);
        _need_scheduling = true;
    }

    actor_base* pop_task() {
        actor_base* front_task;
        if (!_urgent_q.empty()) {
            front_task = _urgent_q.front();
            _urgent_q.pop_front();
        } else if (_comparator == nullptr) {
            front_task = _q.front();
            _q.pop_front();
        } else {
            if (_need_scheduling) {
                std::make_heap(_q.begin(), _q.end(), *_comparator);
                _need_scheduling = false;
            }
            std::pop_heap(_q.begin(), _q.end(), *_comparator);
            front_task = _q.back();
            _q.pop_back();
        }
        return front_task;
    }

    void cancel_all() {
        _urgent_q.for_each([](seastar::task* ut) {
            ut->cancel();
        });
        _q.for_each([](seastar::task* t) {
            t->cancel();
        });
    }

    bool empty() const {
        return (_urgent_q.empty() && _q.empty());
    }

    size_t size() const {
        return (_urgent_q.size() + _q.size());
    }
};

} // namespace hiactor