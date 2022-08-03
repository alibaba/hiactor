/** Copyright 2022 Alibaba Group Holding Limited. All Rights Reserved.
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

#include "actor_harness.hh"
#include <hiactor/core/actor_client.hh>
#include <hiactor/core/actor_factory.hh>

using namespace std::chrono_literals;

enum : uint8_t {
    k_count_check = 0,
    k_count_increase = 1,
};

count_ref::count_ref() : hiactor::reference_base() { actor_type = 0; }

seastar::future<hiactor::Integer> count_ref::check() {
	addr.set_method_actor_tid(0);
    return hiactor::actor_client::request<hiactor::Integer>(addr, k_count_check);
}

seastar::future<hiactor::Integer> count_ref::increase(hiactor::Integer &&amount) {
    return hiactor::actor_client::request<hiactor::Integer, hiactor::Integer>(addr, k_count_increase, std::forward<hiactor::Integer>(amount));
}

seastar::future<hiactor::Integer> count::increase(hiactor::Integer &&amount) {
    auto request = amount.val;
    if (request <= 0) {
        return seastar::make_exception_future<hiactor::Integer>(std::logic_error(
            "The increase amount must be greater than 0."));
    }
    else {
        count_res += request;
        return seastar::make_ready_future<hiactor::Integer>(count_res);
    }
}

seastar::future<hiactor::Integer> count::check() {
    return seastar::make_ready_future<hiactor::Integer>(count_res);
}

seastar::future<hiactor::stop_reaction> count::do_work(hiactor::actor_message* msg) {
	switch (msg->hdr.behavior_tid) {
		case k_count_check: {
			return check().then_wrapped([msg] (seastar::future<hiactor::Integer> fut) {
				if (__builtin_expect(fut.failed(), false)) {
					auto* ex_msg = hiactor::make_response_message(
						msg->hdr.src_shard_id,
						hiactor::simple_string::from_exception(fut.get_exception()),
						msg->hdr.pr_id,
						hiactor::message_type::EXCEPTION_RESPONSE);
					return hiactor::actor_engine().send(ex_msg);
				}
				auto* response_msg = hiactor::make_response_message(
					msg->hdr.src_shard_id, fut.get0(), msg->hdr.pr_id, hiactor::message_type::RESPONSE);
				return hiactor::actor_engine().send(response_msg);
			}).then([] {
				return seastar::make_ready_future<hiactor::stop_reaction>(hiactor::stop_reaction::no);
			});
		}
		case k_count_increase: {
			static auto apply_increase = [] (hiactor::actor_message *a_msg, count *self) {
				if (!a_msg->hdr.from_network) {
					return self->increase(std::move(reinterpret_cast<
						hiactor::actor_message_with_payload<hiactor::Integer>*>(a_msg)->data));
				} else {
					auto* ori_msg = reinterpret_cast<hiactor::actor_message_with_payload<
						hiactor::serializable_queue>*>(a_msg);
					return self->increase(hiactor::Integer::load_from(ori_msg->data));
				}
			};
			return apply_increase(msg, this).then_wrapped([msg] (seastar::future<hiactor::Integer> fut) {
				if (__builtin_expect(fut.failed(), false)) {
					auto* ex_msg = hiactor::make_response_message(
						msg->hdr.src_shard_id,
						hiactor::simple_string::from_exception(fut.get_exception()),
						msg->hdr.pr_id,
						hiactor::message_type::EXCEPTION_RESPONSE);
					return hiactor::actor_engine().send(ex_msg);
				}
				auto* response_msg = hiactor::make_response_message(
					msg->hdr.src_shard_id, fut.get0(), msg->hdr.pr_id, hiactor::message_type::RESPONSE);
				return hiactor::actor_engine().send(response_msg);
			}).then([] {
				return seastar::make_ready_future<hiactor::stop_reaction>(hiactor::stop_reaction::no);
			});
		}
		default: {
			return seastar::make_ready_future<hiactor::stop_reaction>(hiactor::stop_reaction::yes);
		}
	}
}

namespace auto_registration {

hiactor::registration::actor_registration<count> _count_auto_registration(0);

}
