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

#include <hiactor/core/coordinator_worker.hh>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/semaphore.hh>

namespace hiactor {

class actor_smp;

class coordinator {
public:
    struct impl;
private:
    coordinator();
    ~coordinator() = default;
    void launch_worker(seastar::reactor* r);
    seastar::semaphore* get_sem(const std::string& barrier_id, bool global);
public:
    static coordinator& get();

    void set_impl(std::unique_ptr<coordinator::impl> impl);
    void set_sync_size(unsigned sync_size);

    seastar::future<> global_barrier(std::string barrier_guid, bool with_local = true);
    seastar::future<> local_barrier(std::string barrier_guid);
private:
    using sem_map_t = std::unordered_map<std::string, std::unique_ptr<seastar::semaphore>>;
    std::unique_ptr<coordinator_worker> worker_;
    std::unique_ptr<coordinator::impl> impl_;
    sem_map_t local_sems_;
    sem_map_t global_sems_;

    friend class coordinator_pollfn;
    friend class actor_smp;
};

struct coordinator::impl {
    impl() : sync_size_(0) {}
    virtual ~impl() = default;

    virtual int global_barrier(const std::string& barrier_guid) = 0;
    void set_sync_size(unsigned sync_size);
protected:
    unsigned sync_size_;
};

inline
coordinator::coordinator() : worker_(nullptr), impl_(nullptr) {}

inline
seastar::semaphore* coordinator::get_sem(const std::string& barrier_id, bool global) {
    auto& sem_map = global ? global_sems_ : local_sems_;
    if (sem_map.find(barrier_id) == sem_map.end()) {
        sem_map[barrier_id] = std::make_unique<seastar::semaphore>(0);
    }
    return sem_map[barrier_id].get();
}

inline
coordinator& coordinator::get() {
    static coordinator inst;
    return inst;
}

inline
void coordinator::set_impl(std::unique_ptr<coordinator::impl> impl) {
    impl_ = std::move(impl);
}

inline
void coordinator::impl::set_sync_size(unsigned sync_size) {
    sync_size_ = sync_size;
}

} // namespace hiactor
