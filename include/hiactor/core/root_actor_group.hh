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
#include <hiactor/core/actor_message.hh>
#include <hiactor/core/continuation.hh>
#include <hiactor/core/promise_manager.hh>

#include <climits>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <seastar/core/semaphore.hh>

namespace hiactor {

class actor_smp;
class actor_message_queue;
class network_message_queue;
class actor_smp_pollfn;

// Semaphore count type is ssize_t.
const size_t GMaxActiveChildNum = SSIZE_MAX;
const size_t GInitTwoWayPromiseNum = 100;

class root_actor_group final : public actor_base {
    seastar::reactor& _reactor;
    uint32_t _num_actors_managed = 0;
    std::unordered_map<uint64_t, actor_base*> _actor_inst_map;
    pr_manager<actor_message*> _response_pr_manager{GInitTwoWayPromiseNum};
    seastar::semaphore _actor_activation_sem{GMaxActiveChildNum};
    std::atomic<bool> _sleeping{false};
    std::optional<seastar::reactor::poller> _actor_smp_poller = {};
    std::optional<seastar::reactor::poller> _coord_poller = {};
    std::optional<seastar::reactor::poller> _thread_resource_poller = {};
#ifdef HIACTOR_GPU_ENABLE
    seastar::compat::optional<seastar::reactor::poller> _gpu_resource_poller = {};
#endif
public:
    explicit root_actor_group(seastar::reactor& reactor);
    ~root_actor_group() override;

    void cancel_scope(const scope_builder& scope, bool force = true);
    seastar::future<> cancel_scope_request(const scope_builder& scope, bool force = true);

    seastar::future<> send(actor_message* msg);
    void exit(bool force = false);

    bool is_stopping() const {
        return _stop_status != actor_status::RUNNING;
    }

    void wakeup_reactor() {
        _reactor.wakeup();
    }

private:
    void register_actor_pollers();
    void deregister_actor_pollers();
    void add_task(seastar::task* t) override;
    void add_urgent_task(seastar::task* t) override;
    void stopped_actor_enqueue(actor_message* msg);
    void run_and_dispose() noexcept override {}
    void stop_child_actor(byte_t* child_addr, bool force) override;
    void notify_child_stopped() override;
    void stop_actor_system(bool force);
    void clear_actor_system();
    void wait_others_and_exit();
    seastar::future<> receive(actor_message* msg);

    friend class actor_smp;
    friend class actor_message_queue;
    friend class network_message_queue;
    friend class actor_smp_pollfn;
    friend struct actor_client;
};

extern __thread root_actor_group* local_actor_engine;

void allocate_actor_engine();

inline
root_actor_group& actor_engine() {
    return *local_actor_engine;
}

inline
void set_local_execution_context(seastar::execution_context* exec_ctx) {
    seastar::set_local_ec(exec_ctx);
}

inline
seastar::execution_context* get_local_execution_ctx() {
    return seastar::get_local_ec();
}

} // namespace hiactor
