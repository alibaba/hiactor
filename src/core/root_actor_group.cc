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
#include <hiactor/core/actor_factory.hh>
#include <hiactor/core/actor_pollfn.hh>
#include <hiactor/core/coordinator.hh>
#include <hiactor/core/exception.hh>
#include <hiactor/core/local_channel.hh>
#include <hiactor/core/message_reclaimer.hh>
#include <hiactor/core/root_actor_group.hh>
#include <hiactor/net/network_channel.hh>
#include <hiactor/util/data_type.hh>
#include <hiactor/util/machine_info.hh>
#include <hiactor/util/unaligned_int.hh>

namespace hiactor {

root_actor_group::root_actor_group(seastar::reactor& reactor) : actor_base(nullptr), _reactor(reactor) {}

root_actor_group::~root_actor_group() {
    assert(_num_actors_managed == 0);
}

void root_actor_group::register_actor_pollers() {
    _actor_smp_poller = seastar::reactor::poller(std::make_unique<actor_smp_pollfn>(*this));
    if (local_shard_id() == 0) {
        _coord_poller = seastar::reactor::poller(std::make_unique<coordinator_pollfn>());
    }
    if (thread_resource_pool::active()) {
        _thread_resource_poller = seastar::reactor::poller(std::make_unique<thread_resource_pollfn>());
    }
#ifdef HIACTOR_GPU_ENABLE
    _gpu_resource_poller = seastar::reactor::poller(std::make_unique<gpu_resource_pollfn>());
#endif
}

void root_actor_group::deregister_actor_pollers() {
    _actor_smp_poller = {};
    _coord_poller = {};
    _thread_resource_poller = {};
#ifdef HIACTOR_GPU_ENABLE
    _gpu_resource_poller = {};
#endif
}

void root_actor_group::stop_actor_system(bool force) {
    if (_stop_status != actor_status::RUNNING) { return; }
    _stop_status = force ? actor_status::FORCE_STOPPING
                         : actor_status::PEACE_STOPPING;
    for (uint32_t id = 0; id < _response_pr_manager.size(); ++id) {
        if (_response_pr_manager.is_active(id)) {
            _response_pr_manager.set_exception(id, std::make_exception_ptr(task_canceled_exception(
                "Cleaning up uncompleted promise in response_pr_manager "
                "when actor system is stopping!")));
            _response_pr_manager.remove_pr(id);
        }
    }
    for (auto&& pairs : _actor_inst_map) {
        if (pairs.second != nullptr) {
            pairs.second->stop(force);
            pairs.second = nullptr;
        }
    }
    if (_num_actors_managed == 0) {
        wait_others_and_exit();
    }
}

void root_actor_group::clear_actor_system() {
    assert(local_shard_id() == 0);
    actor_smp::cleanup_cpu();
    actor_engine().deregister_actor_pollers();
    do_with(seastar::semaphore(0), [](seastar::semaphore& sem) {
        for (unsigned i = 1; i < local_shard_count(); i++) {
            seastar::smp::submit_to(i, []() {
                actor_smp::cleanup_cpu();
                actor_engine().deregister_actor_pollers();
            }).then([&sem]() {
                sem.signal();
            });
        }
        return sem.wait(local_shard_count() - 1);
    });
    thread_resource_pool::stop();
#ifdef HIACTOR_GPU_ENABLE
    gpu_resource_pool::stop();
#endif
}

void root_actor_group::wait_others_and_exit() {
    coordinator::get().global_barrier("HIACTOR_PEACE_STOPPED").then([this] {
        if (local_shard_id() == 0) {
            clear_actor_system();
            seastar::engine().exit(0);
        }
    });
}

void root_actor_group::stopped_actor_enqueue(actor_message* msg) {
    /// Only send ex-response for request message.
    if (msg->hdr.pr_id != 0) {
        auto* ex_response_msg = make_response_message(
            msg->hdr.src_shard_id,
            simple_string("Unable to send message, the target actor has been canceled!"),
            msg->hdr.pr_id, message_type::EXCEPTION_RESPONSE);
        actor_engine().send(ex_response_msg);
    }
    delete msg;
}

void root_actor_group::add_task(seastar::task* t) {
    seastar::engine().add_task(t);
}

void root_actor_group::add_urgent_task(seastar::task* t) {
    seastar::engine().add_urgent_task(t);
}

seastar::future<> root_actor_group::receive(actor_message* msg) {
    auto& addr = msg->hdr.addr;
    /// destination is the root actor
    if (addr.length == _total_addr_length) {
        switch (msg->hdr.m_type) {
            case message_type::RESPONSE: {
                auto pr_id = msg->hdr.pr_id;
                if (_response_pr_manager.is_active(pr_id)) {
                    _response_pr_manager.set_value(pr_id, msg);
                } else {
                    delete msg;
                }
                break;
            }
            case message_type::EXCEPTION_RESPONSE: {
                auto pr_id = msg->hdr.pr_id;
                if (_response_pr_manager.is_active(pr_id)) {
                    auto ex_content = std::move(reinterpret_cast<actor_message_with_payload<simple_string>*>(msg)->data);
                    _response_pr_manager.set_exception(
                        pr_id, std::make_exception_ptr(actor_method_exception(ex_content.str, ex_content.len)));
                }
                delete msg;
                break;
            }
            case message_type::PEACE_STOP: {
                stop_actor_system(false);
                reclaim_actor_message(msg);
                break;
            }
            case message_type::FORCE_STOP: {
                stop_actor_system(true);
                reclaim_actor_message(msg);
                break;
            }
            default:
                assert(false);
        }
        return seastar::make_ready_future<>();
    }

    /// destination is an actor
    auto child_id = load_unaligned_int_partial<uint64_t>(addr.data + _total_addr_length, GLocalActorAddrLength);
    auto search = _actor_inst_map.find(child_id);
    if (search != _actor_inst_map.end()) {
        /// actor is disabled, delete the message
        if (!search->second) {
            stopped_actor_enqueue(msg);
            return seastar::make_ready_future<>();
        }
        actor_base* dest_actor = (addr.length == search->second->get_address_length()) ?
            search->second : search->second->get_actor_local(msg->hdr);
        if (dest_actor != nullptr) {
            if (msg->hdr.m_type == message_type::USER) {
                dest_actor->enque_message(msg);
            } else {
                dest_actor->enque_urgent_message(msg);
            }
        } else {
            /// actor is disabled, delete the message
            stopped_actor_enqueue(msg);
        }
        return seastar::make_ready_future<>();
    }

    if (_stop_status != actor_status::RUNNING) {
        stopped_actor_enqueue(msg);
        return seastar::make_ready_future<>();
    }

    /// actor not exists, create a new one
    auto* child_head = addr.data + _total_addr_length;
    auto actor_type = load_unaligned_int<uint16_t>(child_head);
    actor_base* child = actor_factory::get().create(actor_type, this, child_head);
    _actor_inst_map[child_id] = child;
    ++_num_actors_managed;
    auto sem_f = _actor_activation_sem.wait(1);
    if (sem_f.available()) {
        child->schedule();
    } else {
        auto activate_actor_func = [child](const seastar::future_state<seastar::future<>::value_type>& state) {
            child->schedule();
        };
        using continuationized_func = continuation<std::function<void(const seastar::future_state<seastar::future<>::value_type>& state)>>;
        seastar::internal::set_callback(sem_f, new continuationized_func(std::move(activate_actor_func)));
    }

    actor_base* dest_actor = (addr.length == child->get_address_length()) ? child : child->get_actor_local(msg->hdr);
    if (msg->hdr.m_type == message_type::USER) {
        dest_actor->enque_message(msg);
    } else {
        dest_actor->enque_urgent_message(msg);
    }
    return seastar::make_ready_future<>();
}

void root_actor_group::cancel_scope(const scope_builder& scope, bool force) {
    auto m_type = force ? message_type::FORCE_STOP : message_type::PEACE_STOP;
    for (uint32_t sid = 0; sid < global_shard_count(); ++sid) {
        address dst_addr = scope.addr;
        memcpy(dst_addr.data, &sid, GShardIdInBytes);
        auto* msg = make_system_message(dst_addr, m_type);
        actor_engine().send(msg);
    }
}

seastar::future<> root_actor_group::cancel_scope_request(const scope_builder& scope, bool force) {
    return seastar::parallel_for_each(
        boost::irange(0u, global_shard_count()),
        [this, &scope, force](unsigned dst_sid) {
            auto m_type = force ? message_type::FORCE_STOP : message_type::PEACE_STOP;
            address dst_addr = scope.addr;
            memcpy(dst_addr.data, &dst_sid, GShardIdInBytes);

            auto src_sid = global_shard_id();
            auto pr_id = _response_pr_manager.acquire_pr();
            auto* msg = make_request_message(dst_addr, 0, src_sid, pr_id, m_type);
            return seastar::when_all(
                actor_engine().send(msg), actor_engine()._response_pr_manager.get_future(pr_id)
            ).then([pr_id](std::tuple<seastar::future<>, seastar::future<actor_message*>> tup) {
                actor_engine()._response_pr_manager.remove_pr(pr_id);
                auto& send_fut = std::get<0>(tup);
                if (__builtin_expect(send_fut.failed(), false)) {
                  return seastar::make_exception_future<>(send_fut.get_exception());
                }
                auto& res_fut = std::get<1>(tup);
                if (__builtin_expect(res_fut.failed(), false)) {
                  return seastar::make_exception_future<>(res_fut.get_exception());
                }
                auto* res_msg = res_fut.get0();
                delete res_msg;
                return seastar::make_ready_future<>();
            });
        });
}

void root_actor_group::exit(bool force) {
    auto m_type = force ? message_type::FORCE_STOP : message_type::PEACE_STOP;
    for (uint32_t sid = 0; sid < global_shard_count(); ++sid) {
        address dst_addr{};
        memcpy(dst_addr.data, &sid, GShardIdInBytes);
        dst_addr.length = GShardIdInBytes;
        auto* msg = make_system_message(dst_addr, m_type);
        actor_engine().send(msg);
    }
}

seastar::future<> root_actor_group::send(actor_message* msg) {
    static thread_local const unsigned this_gsid = global_shard_id();
    auto dest_shard_id = msg->hdr.addr.get_shard_id();
    if (dest_shard_id != this_gsid) {
        if (machine_info::is_local(dest_shard_id)) {
            return local_channel::send(msg);
        } else {
            return network_channel::send(msg);
        }
    }
    return receive(msg);
}

/// Only existing child actor group calls this function to terminate itself
void root_actor_group::stop_child_actor(byte_t* child_addr, bool force) {
    auto child_id = load_unaligned_int_partial<uint64_t>(child_addr, GLocalActorAddrLength);
    auto search = _actor_inst_map.find(child_id);
    assert (search != _actor_inst_map.end());
    if (search->second) {
        search->second->stop(force);
        _actor_inst_map[child_id] = nullptr;
    }
}

void root_actor_group::notify_child_stopped() {
    if (--_num_actors_managed == 0 && _stop_status != actor_status::RUNNING) {
        wait_others_and_exit();
    }
    _actor_activation_sem.signal(1);
}

// [DAIL] hold root_actor_group.
struct rag_deleter {
    void operator()(root_actor_group* p) {
        p->~root_actor_group();
        free(p);
    }
};

thread_local std::unique_ptr<root_actor_group, rag_deleter> rag_holder;

void allocate_actor_engine() {
    assert(!rag_holder);
    void* buf;
    int r = posix_memalign(&buf, seastar::cache_line_size, sizeof(root_actor_group));
    assert(r == 0);
    local_actor_engine = reinterpret_cast<root_actor_group*>(buf);
    new (buf) root_actor_group(seastar::engine());
    rag_holder.reset(local_actor_engine);
}

__thread root_actor_group* local_actor_engine{nullptr};

} // namespace hiactor
