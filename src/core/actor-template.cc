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

#include <hiactor/core/actor-template.hh>

namespace hiactor {

template <>
uint16_t get_actor_group_type_id<actor_group>() {
    return 0xF000;
}

registration::actor_registration<actor_group> _default_actor_group_registration(0xF000);

template <>
uint16_t get_actor_group_type_id<schedulable_actor_group>() {
    return 0xF001;
}

registration::actor_registration<schedulable_actor_group> _schedulable_actor_group_registration(0xF001);

/// actor group impls

void actor_group::process_message(actor_message* msg) {
    switch (msg->hdr.m_type) {
        case message_type::PEACE_STOP : {
            dynamic_cast<actor_base*>(_ec)->stop_child_actor(_address, false);
            cancel_request_sid = msg->hdr.src_shard_id;
            cancel_request_pr_id = msg->hdr.pr_id;
            break;
        }
        case message_type::FORCE_STOP : {
            dynamic_cast<actor_base*>(_ec)->stop_child_actor(_address, true);
            cancel_request_sid = msg->hdr.src_shard_id;
            cancel_request_pr_id = msg->hdr.pr_id;
            break;
        }
        default :
            assert(false);
            break;
    }
    reclaim_actor_message(msg);
}

void actor_group::add_task(seastar::task* t) {
    _task_queue.push_task(dynamic_cast<actor_base*>(t));
    activate();
}

void actor_group::add_urgent_task(seastar::task* t) {
    _task_queue.push_urgent_task(dynamic_cast<actor_base*>(t));
    activate();
}

actor_base* actor_group::get_actor_local(actor_message::header& hdr) {
    auto& addr = hdr.addr;
    auto child_id = load_unaligned_int_partial<uint64_t>(addr.data + _total_addr_length, GLocalActorAddrLength);
    auto search = _actor_inst_map.find(child_id);
    if (search != _actor_inst_map.end()) {
        /// child actor is disabled, return nullptr
        if (!search->second) {
            return nullptr;
        }
        actor_base* dest_actor = (addr.length == search->second->get_address_length()) ?
            search->second : search->second->get_actor_local(hdr);
        return dest_actor;
    } else {
        auto* child_head = addr.data + _total_addr_length;
        auto actor_type = load_unaligned_int<uint16_t>(child_head);
        actor_base* child = actor_factory::get().create(actor_type, this, child_head);
        _actor_inst_map[child_id] = child;
        ++_num_actors_managed;

        auto sem_f = _actor_activation_sem.wait(1);
        if (sem_f.available()) {
            child->schedule();
        } else {
            auto activate_actor_func = [child](const seastar::future_state<int>&& state) {
                child->schedule();
            };
            using continuationized_func = continuation<std::function<void(const seastar::future_state<int>&&)>, int>;
            seastar::internal::set_callback(
                sem_f, new continuationized_func(std::move(activate_actor_func)));
        }

        if (addr.length == child->get_address_length()) {
            return child;
        } else {
            return child->get_actor_local(hdr);
        }
    }
}

void actor_group::run_and_dispose() noexcept {
    auto prev_exec_ctx = get_local_execution_ctx();
    set_local_execution_context(this);
    set_timer();

    if (force_stopping()) {
        _task_queue.cancel_all();
    }

    while (!_mailbox.empty()) {
        auto msg = deque_message();
        process_message(msg);
        advance_actor_clock();
        if (need_yield()) { goto FINAL; }
    }

    while (!_task_queue.empty()) {
        auto* actor_task = _task_queue.pop_task();
        set_actor_quota(_unused_quota);
        actor_task->run_and_dispose();
        if (need_yield()) { break; }
    }

    FINAL:
    set_local_execution_context(prev_exec_ctx);
    if (stopping() && !_num_actors_managed && _task_queue.empty() && _mailbox.empty()) {
        dynamic_cast<actor_base*>(_ec)->notify_child_stopped();
        if (cancel_request_pr_id != 0) {
            auto* response_msg = make_response_message(
                cancel_request_sid, cancel_request_pr_id, message_type::RESPONSE);
            actor_engine().send(response_msg);
        }
        delete this;
        return;
    }

    set_activatable();
    if (!_mailbox.empty() || !_task_queue.empty()) {
        activate();
    }
}

void actor_group::stop(bool force) {
    if (stopping()) { return; }
    if (force) {
        _stop_status = actor_status::FORCE_STOPPING;
        if (_actor_activation_sem.waiters() != 0) { _actor_activation_sem.broken(); }
        stop_all_children(force);
        clean_mailbox();
    } else {
        _stop_status = actor_status::PEACE_STOPPING;
        stop_all_children(force);
    }
    if (!_num_actors_managed && activatable()) {
        activate();
    }
}

void actor_group::stop_all_children(bool force) {
    for (auto&& pairs : _actor_inst_map) {
        if (pairs.second != nullptr) {
            address null_addr{};
            auto m_type = force ? message_type::FORCE_STOP : message_type::PEACE_STOP;
            auto* stop_msg = make_system_message(null_addr, m_type);
            pairs.second->enque_urgent_message(stop_msg);
        }
    }
}

void actor_group::stop_child_actor(byte_t* child_addr, bool force) {
    auto child_id = load_unaligned_int_partial<uint64_t>(child_addr, GLocalActorAddrLength);
    auto search = _actor_inst_map.find(child_id);
    if (search->second) {
        search->second->stop(force);
        _actor_inst_map[child_id] = nullptr;
    }
}

void actor_group::notify_child_stopped() {
    --_num_actors_managed;
    _actor_activation_sem.signal(1);
}

} // namespace hiactor
