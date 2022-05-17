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

#include <hiactor/core/coordinator.hh>
#include <hiactor/core/gpu_resource_pool.hh>
#include <hiactor/core/local_channel.hh>
#include <hiactor/core/root_actor_group.hh>
#include <hiactor/core/thread_resource_pool.hh>
#include <hiactor/net/network_channel.hh>

#include <seastar/core/reactor.hh>
#include <seastar/core/systemwide_memory_barrier.hh>

namespace hiactor {

/// actor smp pollfn
class actor_smp_pollfn final : public seastar::pollfn {
    root_actor_group& _rag;
public:
    explicit actor_smp_pollfn(root_actor_group& rag) : _rag(rag) {}
    bool poll() final {
        return (local_channel::poll_queues() | network_channel::poll_queues());
    }
    bool pure_poll() final {
        return (local_channel::pure_poll_queues() | network_channel::pure_poll_queues());
    }
    bool try_enter_interrupt_mode() final {
        _rag._sleeping.store(true, std::memory_order_relaxed);
        bool barrier_done = seastar::try_systemwide_memory_barrier();
        if (!barrier_done) {
            _rag._sleeping.store(false, std::memory_order_relaxed);
            return false;
        }
        if (poll()) {
            // raced
            _rag._sleeping.store(false, std::memory_order_relaxed);
            return false;
        }
        return true;
    }
    void exit_interrupt_mode() final {
        _rag._sleeping.store(false, std::memory_order_relaxed);
    }
};

/// coordinator pollfn
class coordinator_pollfn final : public seastar::pollfn {
public:
    bool poll() final {
        return bool(coordinator::get().worker_->complete());
    }
    bool pure_poll() final {
        return poll();
    }
    bool try_enter_interrupt_mode() override {
        coordinator::get().worker_->enter_interrupt_mode();
        if (poll()) {
            // raced
            coordinator::get().worker_->exit_interrupt_mode();
            return false;
        }
        return true;
    }
    void exit_interrupt_mode() final {
        coordinator::get().worker_->exit_interrupt_mode();
    }
};

/// thread resource pool pollfn
class thread_resource_pollfn final : public seastar::pollfn {
public:
    bool poll() final {
        return thread_resource_pool::poll_queues();
    }
    bool pure_poll() final {
        return thread_resource_pool::pure_poll_queues();
    }
    bool try_enter_interrupt_mode() final {
        thread_resource_pool::enter_interrupt_mode();
        if (poll()) {
            // raced
            thread_resource_pool::exit_interrupt_mode();
            return false;
        }
        return true;
    }
    void exit_interrupt_mode() final {
        thread_resource_pool::exit_interrupt_mode();
    }
};

/// gpu resource pool pollfn
class gpu_resource_pollfn final : public seastar::pollfn {
public:
    bool poll() final {
        return gpu_resource_pool::poll_queues();
    }
    bool pure_poll() final {
        return gpu_resource_pool::pure_poll_queues();

    }
    bool try_enter_interrupt_mode() final {
        return true;
    }
    void exit_interrupt_mode() final {
    }
};

}  // namespace hiactor
