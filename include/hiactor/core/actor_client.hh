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

#include <hiactor/core/actor_message.hh>
#include <hiactor/core/exception.hh>
#include <hiactor/core/root_actor_group.hh>

#include <seastar/core/when_all.hh>

namespace hiactor {

struct actor_client {
    static void send(const address& addr, uint8_t behv_tid, message_type mtype = message_type::USER) {
        auto src_sid = hiactor::global_shard_id();
        auto* msg = make_one_way_request_message(addr, behv_tid, src_sid, mtype);
        actor_engine().send(msg);
    }

    template <typename T, typename = typename std::enable_if<std::is_rvalue_reference<T&&>::value>::type>
    static void send(const address& addr, uint8_t behv_tid, T&& data, message_type mtype = message_type::USER) {
        auto src_sid = hiactor::global_shard_id();
        auto* msg = make_one_way_request_message(addr, behv_tid, std::forward<T>(data), src_sid, mtype);
        actor_engine().send(msg);
    }

    template <typename retT, typename T,
        typename = typename std::enable_if<std::is_rvalue_reference<T&&>::value>::type>
    static seastar::future<retT> request(const address& addr, uint8_t behv_tid, T&& data, message_type mtype = USER) {
        auto pr_id = actor_engine()._response_pr_manager.acquire_pr();
        auto src_sid = hiactor::global_shard_id();
        auto* msg = make_request_message(addr, behv_tid, std::forward<T>(data), src_sid, pr_id, mtype);
        return send_and_wait<retT>(msg, pr_id);
    }

    template <typename retT>
    static seastar::future<retT> request(const address& addr, uint8_t behv_tid, message_type mtype = USER) {
        auto pr_id = actor_engine()._response_pr_manager.acquire_pr();
        auto src_sid = hiactor::global_shard_id();
        auto* msg = make_request_message(addr, behv_tid, src_sid, pr_id, mtype);
        return send_and_wait<retT>(msg, pr_id);
    }

    template <typename retT>
    static seastar::future<retT> send_and_wait(actor_message* msg, uint32_t pr_id) {
        return seastar::when_all(
            actor_engine().send(msg), actor_engine()._response_pr_manager.get_future(pr_id)
        ).then([pr_id] (std::tuple<seastar::future<>, seastar::future<actor_message*>> tup) {
            actor_engine()._response_pr_manager.remove_pr(pr_id);
            auto& send_fut = std::get<0>(tup);
            if (__builtin_expect(send_fut.failed(), false)) {
              return seastar::make_exception_future<retT>(send_fut.get_exception());
            }
            auto& res_fut = std::get<1>(tup);
            if (__builtin_expect(res_fut.failed(), false)) {
              return seastar::make_exception_future<retT>(res_fut.get_exception());
            }
            auto* res_msg = res_fut.get0();
            if (!res_msg->hdr.from_network) {
                auto res_data = std::move(reinterpret_cast<actor_message_with_payload<retT>*>(res_msg)->data);
                delete res_msg;
                return seastar::make_ready_future<retT>(std::move(res_data));
            }
            else {
                auto* ori_msg = reinterpret_cast<actor_message_with_payload<serializable_queue>*>(res_msg);
                auto res_data = retT::load_from(ori_msg->data);
                delete ori_msg;
                return seastar::make_ready_future<retT>(std::move(res_data));
            }
        });
    }
};

} // namespace hiactor
