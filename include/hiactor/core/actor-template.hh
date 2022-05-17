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

/// Write your customized actor with reentrancy by inheriting this class.
///
/// The `do_work` method in derived actor does not need to implemented but
/// only needs to be declared as override or final, the hiactor codegen tool
/// will generate the derived `do_work` implementations.
template <uint32_t MaxConcurrency>
class reentrant_actor : public actor_base {
    seastar::circular_buffer<seastar::task*> _task_queue{};
    uint32_t _max_concurrency = MaxConcurrency;
    uint32_t _cur_concurrency = 0;
private:
    void add_task(seastar::task* t) override;
    void add_urgent_task(seastar::task* t) override;
    void run_and_dispose() noexcept override;
    void stop(bool force) override;
    void clean_task_queue();
public:
    reentrant_actor(actor_base* exec_ctx, const byte_t* addr) : actor_base(exec_ctx, addr) {}
    ~reentrant_actor() override = default;
    virtual seastar::future<stop_reaction> do_work(actor_message* msg) = 0;
};

/// Inherit a stateless actor to run concurrent stateless operations nad
/// allow unlimited reentrancy.
using stateless_actor = reentrant_actor<UINT32_MAX>;

/// Inherit a stateful actor to run serialized operations that manipulates at least
/// one centralized state, which means that reentrancy is not allowed in this actor.
///
/// States are hold in derived actors.
using stateful_actor = reentrant_actor<1>;


/// reentrant actor impls

template <uint32_t MaxConcurrency>
inline
void reentrant_actor<MaxConcurrency>::run_and_dispose() noexcept {
    auto prev_exec_ctx = get_local_execution_ctx();
    set_local_execution_context(this);
    set_timer();

    if (force_stopping()) {
        _task_queue.for_each([](seastar::task* t) {
            t->cancel();
        });
    }

    while (!_task_queue.empty()) {
        auto* task = _task_queue.front();
        _task_queue.pop_front();
        task->run_and_dispose();
        advance_actor_clock();
        if (need_yield()) { goto FINAL; }
    }

    if (_cur_concurrency == _max_concurrency) {  // reaches the concurrency limit
        set_activatable();
        set_local_execution_context(prev_exec_ctx);
        return;
    }

    while (!_mailbox.empty()) {
        actor_message* msg = deque_message();
        switch (msg->hdr.m_type) {
            case message_type::USER: {
                auto result_f = do_work(msg);
                if (!result_f.available()) {
                    ++_cur_concurrency;
                    auto post_work_func = [this, msg](const seastar::future_state<stop_reaction>& state) {
                        --_cur_concurrency;
                        reclaim_actor_message(msg);
                        if (state.failed()) {
                            log_exception(state.get_exception());
                        } else if (std::get<0>(state.get()) == stop_reaction::yes) {
                            dynamic_cast<actor_base*>(_ec)->stop_child_actor(_address, true);
                        }
                    };
                    using continuationized_func =
                        continuation<std::function<void(seastar::future_state<stop_reaction>)>, stop_reaction>;
                    seastar::internal::set_callback(
                        result_f, new continuationized_func(std::move(post_work_func)));
                    if (_cur_concurrency == _max_concurrency) { goto FINAL; }
                    break;
                }
                // do_work() completes without blocking
                reclaim_actor_message(msg);
                if (result_f.failed()) {
                    log_exception(result_f.get_exception());
                } else if (result_f.get0() == stop_reaction::yes) {
                    dynamic_cast<actor_base*>(_ec)->stop_child_actor(_address, true);
                }
                break;
            }
            case message_type::PEACE_STOP : {
                dynamic_cast<actor_base*>(_ec)->stop_child_actor(_address, false);
                reclaim_actor_message(msg);
                break;
            }
            case message_type::FORCE_STOP : {
                dynamic_cast<actor_base*>(_ec)->stop_child_actor(_address, true);
                reclaim_actor_message(msg);
                break;
            }
            default:
                assert(false);
                reclaim_actor_message(msg);
        }
        advance_actor_clock();
        if (need_yield()) { break; }
    }

    FINAL:
    set_local_execution_context(prev_exec_ctx);
    if (stopping() && !_cur_concurrency && _mailbox.empty()) {
        dynamic_cast<actor_base*>(_ec)->notify_child_stopped();
        delete this;
        return;
    }

    set_activatable();
    if (!_task_queue.empty() || (!_mailbox.empty() && _cur_concurrency < _max_concurrency)) {
        activate();
    }
}

template <uint32_t MaxConcurrency>
inline
void reentrant_actor<MaxConcurrency>::add_task(seastar::task* t) {
    _task_queue.push_back(t);
    activate();
}

template <uint32_t MaxConcurrency>
inline
void reentrant_actor<MaxConcurrency>::add_urgent_task(seastar::task* t) {
    _task_queue.push_front(t);
    activate();
}

template <uint32_t MaxConcurrency>
inline
void reentrant_actor<MaxConcurrency>::stop(bool force) {
    if (stopping()) { return; }
    if (force) {
        _stop_status = actor_status::FORCE_STOPPING;
        clean_mailbox();
        clean_task_queue();
    } else {
        _stop_status = actor_status::PEACE_STOPPING;
    }
    if (activatable() /** not in the task queue */ && !_cur_concurrency /** no flying chain */) {
        activate();
    }
}

template <uint32_t MaxConcurrency>
inline
void reentrant_actor<MaxConcurrency>::clean_task_queue() {
    assert(_mailbox.empty() && force_stopping());
    _task_queue.for_each([](seastar::task* t) {
        t->cancel();
    });
    while (!_task_queue.empty()) {
        auto* t = _task_queue.front();
        _task_queue.pop_front();
        t->run_and_dispose();
    }
}

} // namespace hiactor
