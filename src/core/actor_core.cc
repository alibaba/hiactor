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

#include <hiactor/core/actor_core.hh>
#include <hiactor/core/exception.hh>
#include <hiactor/core/local_channel.hh>
#include <hiactor/core/message_reclaimer.hh>
#include <hiactor/core/root_actor_group.hh>

#include <seastar/core/print.hh>

namespace hiactor {

actor_base::actor_base(actor_base* exec_ctx, const byte_t* addr) : seastar::task(exec_ctx) {
    _total_addr_length = exec_ctx->get_address_length() + GLocalActorAddrLength;
    memcpy(_address, addr, GLocalActorAddrLength);
}

/// This constructor is only used by the root actor of each shard
actor_base::actor_base(actor_base* exec_ctx) : seastar::task(exec_ctx) {
    _total_addr_length = GShardIdInBytes;
}

actor_base::~actor_base() {
    _mailbox.for_each([](actor_message* msg) { delete msg; });
}

void actor_base::enque_message(actor_message* message) {
    _mailbox.push_back(message);
    activate();
}

void actor_base::enque_urgent_message(actor_message* message) {
    _mailbox.push_front(message);
    activate();
}

actor_message* actor_base::deque_message() {
    auto message = _mailbox.front();
    _mailbox.pop_front();
    return message;
}

void actor_base::activate() {
    if (activatable()) {
        _sched_status = schedule_status::SCHEDULED;
        seastar::schedule(this);
    }
}

void actor_base::set_timer() {
    if (_ec == &actor_engine()) {
        // set initial time quota
        actor_clock = std::chrono::steady_clock::now();
        set_actor_quota(default_actor_quota);
    }
    _unused_quota = read_actor_quota();
    _stop_time = read_actor_clock() + _unused_quota;
}

bool actor_base::need_yield() {
    if (seastar::need_preempt()) { return true; }
    _unused_quota = std::chrono::duration_cast<clock_unit>(_stop_time - read_actor_clock());
    return _unused_quota.count() <= 0;
}

void actor_base::log_exception(std::exception_ptr eptr) {
    const char* ex_info;
    try {
        std::rethrow_exception(std::move(eptr));
    } catch (task_canceled_exception& tc_ex) {
        ex_info = tc_ex.what();
#ifndef HIACTOR_DEBUG
        return;
#endif
    } catch (std::exception& ex) {
        ex_info = ex.what();
    } catch (...) {
        ex_info = "unknown exception";
    }
    std::cerr << seastar::format(" Actor {} got exception {}",
                                 char_arr_to_str(_address, GLocalActorAddrLength), ex_info)
              << std::endl;
}

scope_builder actor_base::get_scope() {
    scope_builder scope(local_shard_id());
    auto layer_num = ((_total_addr_length - GShardIdInBytes - GLocalActorAddrLength) / GLocalActorAddrLength);
    auto* actor_g = dynamic_cast<actor_base*>(ec());
    for (auto i = layer_num; i > 0; i--) {
        memcpy(scope.addr.data + GShardIdInBytes + (i - 1) * GLocalActorAddrLength,
               &(actor_g->_address), GLocalActorAddrLength);
        actor_g = dynamic_cast<actor_base*>(actor_g->ec());
    }
    scope.addr.length = (_total_addr_length - GLocalActorAddrLength);
    return scope;
}

actor_base* actor_base::get_actor_local(actor_message::header& hdr) {
    //TODO[DAIL] consider the case that the destination address is longer then this actor's address
    return this;
}

void actor_base::clean_mailbox() {
    while (!_mailbox.empty()) {
        actor_message* msg = deque_message();
        reclaim_actor_message(msg);
    }
}

} // namespace hiactor
