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

#include <hiactor/core/coordinator.hh>
#include <hiactor/core/shard-config.hh>

#include <seastar/core/print.hh>

namespace hiactor {

void coordinator::launch_worker(seastar::reactor* r) {
    worker_ = std::make_unique<coordinator_worker>(r);
}

seastar::future<> coordinator::global_barrier(std::string barrier_id, bool with_local) {
    return seastar::do_with(std::move(barrier_id), [this, with_local](std::string& barrier_id) {
        if (local_shard_id() != 0 && with_local) {
            return seastar::smp::submit_to(0, [&barrier_id, this] {
                get_sem(barrier_id, true)->signal();
            });
        } else {
            auto fut = with_local ?
                get_sem(barrier_id, true)->wait(local_shard_count() - 1) : seastar::make_ready_future<>();
            return fut.then([&barrier_id, this] {
                assert(worker_ && "coordinator worker is not ready.\n");
                return worker_->submit<int>([&barrier_id, this] {
                    if (impl_) {
                        return impl_->global_barrier(barrier_id);
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    return 0;
                });
            }).then([&barrier_id, this](auto) {
                global_sems_.erase(barrier_id);
            });
        }
    });
}

seastar::future<> coordinator::local_barrier(std::string barrier_id) {
    return seastar::do_with(std::move(barrier_id), [this](std::string& barrier_id) {
        if (local_shard_id() != 0) {
            return seastar::smp::submit_to(0, [&barrier_id, this] {
                get_sem(barrier_id, false)->signal();
            });
        } else {
            return get_sem(barrier_id, false)->wait(local_shard_count() - 1).then([&barrier_id, this] {
                local_sems_.erase(barrier_id);
            });
        }
    });
}

void coordinator::set_sync_size(unsigned sync_size) {
    if (impl_) {
        impl_->set_sync_size(sync_size);
    } else {
        fmt::print("Warning: unable to set sync size(coordinator::impl_ is nullptr).\n");
    }
}

} // namespace hiactor
