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

#include <hiactor/core/actor_factory.hh>
#include <hiactor/core/actor_scheduling.hh>
#include <hiactor/core/message_reclaimer.hh>
#include <hiactor/core/root_actor_group.hh>

#include <unordered_map>

namespace hiactor {

/// An actor group manages all its child actors and child actor groups hierarchically.
///
/// This class is the default actor group type, actor tasks are executed in the order
/// in which they are activated, and no scheduling will occur.
class actor_group : public actor_base {
    uint32_t _num_actors_managed = 0;
    std::unordered_map<uint64_t, actor_base*> _actor_inst_map{};
    seastar::semaphore _actor_activation_sem{GMaxActiveChildNum};
    uint32_t cancel_request_sid = UINT32_MAX;
    uint32_t cancel_request_pr_id = 0;
protected:
    schedulable_actor_task_queue _task_queue{};
private:
    void add_task(seastar::task* t) override;
    void add_urgent_task(seastar::task* t) override;
    void process_message(actor_message* msg);
    void run_and_dispose() noexcept override;
    void stop(bool force) override;
    void stop_all_children(bool force);

    /// Recursively looking for the destination actor for the incoming message;
    /// If the destination actor is disabled, return nullptr;
    /// If the destination actor or its ancestor not exists, create an inactive actor.
    actor_base* get_actor_local(actor_message::header& hdr) override;

    /// Child actor/actor group calls this function directly to notify its termination
    /// \param child_addr: pointer to the address of child actor in the format of byte array
    /// \param force: if `true`, the child actor will be stopped without waiting its pending tasks to be finished.
    void stop_child_actor(byte_t* child_addr, bool force) override;

    /// Child actor/actor group calls this function to notify it has been terminated
    void notify_child_stopped() override;
public:
    actor_group(actor_base* exec_ctx, const byte_t* addr) : actor_base(exec_ctx, addr) {}
};

/// Inherit this class to define a customized actor group，with the ability to schedule
/// the execution of child actors/actor groups.
///
/// The scheduling comparing function should be overwritten in the derived class to define
/// the specified scheduling policy.
class schedulable_actor_group : public actor_group {
public:
    schedulable_actor_group(actor_base* exec_ctx, const byte_t* addr) : actor_group(exec_ctx, addr) {
        _task_queue.set_comparator(new scheduling_comparator(this));
    }

    /// Comparator Func of two scheduling actors，needs to be overwritten in derived actor groups.
    /// \return `true`  : the priority of actor task a is lower than actor task b.
    /// \return `false` : the priority of actor task a is larger than or equal to actor task b.
    virtual bool compare(const actor_base* a, const actor_base* b) const {
        return false;
    }
};

inline
bool scheduling_comparator::operator()(const actor_base* a, const actor_base* b) const {
    return _group->compare(a, b);
}

/// Write your customized actor by inheriting this class.
class actor : public actor_base {
    seastar::circular_buffer<seastar::task*> _task_queue{};
    uint32_t _max_concurrency = UINT32_MAX;
    uint32_t _cur_concurrency = 0;
private:
    void add_task(seastar::task* t) override;
    void add_urgent_task(seastar::task* t) override;
    void run_and_dispose() noexcept override;
    void stop(bool force) override;
    void clean_task_queue();
public:
    actor(actor_base* exec_ctx, const byte_t* addr, bool reentrant = true);
    ~actor() override = default;

    /// Set the concurrency for max concurrent reentrant tasks.
    /// If set to "1", this actor cannot be reentrant.
    /// If set to "UINT32_MAX", this actor can be reentrant without limits.
    void set_max_concurrency(uint32_t concurrency);

    /// The `do_work` method in derived actor does not need to implemented but
    /// only needs to be declared as override or final, the hiactor codegen tool
    /// will generate the derived `do_work` implementations.
    virtual seastar::future<stop_reaction> do_work(actor_message* msg) = 0;
};

} // namespace hiactor
