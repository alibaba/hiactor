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

#include <hiactor/core/exception.hh>
#include <hiactor/core/promise_manager.hh>
#include <hiactor/core/shard-config.hh>
#include <hiactor/cuda/device.h>

#include <functional>
#include <unordered_set>
#include <boost/lockfree/queue.hpp>

namespace hiactor {

class gpu_resource_pollfn;

class gpu_resource_pool {
private:
    struct stream_unit {
        cuda_stream_handle stream;
        std::atomic<uint32_t> count{};
        explicit stream_unit(cuda_stream_handle stream, uint32_t init_count = 0)
            : stream(stream), count(init_count) {}
        stream_unit(stream_unit&& other) noexcept
            : stream(other.stream), count(other.count.load()) {}
        stream_unit& operator=(stream_unit&& other) noexcept {
            if (this != &other) {
                stream = other.stream;
                count.store(other.count.load());
            }
            return *this;
        }
    };
    static std::vector<std::vector<stream_unit> > _streams;
    static std::vector<std::unique_ptr<boost::lockfree::queue<uint32_t> > > _candidates;

    struct event_pr {
        cuda_event_handle event;
        uint32_t pr_id;
        event_pr(cuda_event_handle event, uint32_t pr_id) : event(event), pr_id(pr_id) {}
        bool operator==(const event_pr& ep) const { return (this->pr_id == ep.pr_id); }
    };
    struct event_pr_hash_func {
        size_t operator()(const event_pr& ep) const { return ep.pr_id; }
    };
    using event_pr_set = std::unordered_set<event_pr, event_pr_hash_func>;
    struct eps_deleter {
        void operator()(event_pr_set** eps) const;
    };
    static std::vector<std::unique_ptr<pr_manager<> > > _pr_manager;
    static std::unique_ptr<event_pr_set*[], eps_deleter> _event_pr_pool;

    static uint32_t _device_num;
    static uint32_t _stream_num_per_device;
    static uint32_t _event_num_per_stream;
    static int64_t _sleep_duration_in_microseconds;

public:
    static void configure(const boost::program_options::variables_map& configs);
    static void stop();

    template <typename... Args>
    static seastar::future<> submit_work(
        uint32_t device_id, std::function<void(cuda_stream_handle, Args...)>&& gpu_func, Args... args) {
        check_available_or_abort();
        if (device_id >= _device_num) {
            return seastar::make_exception_future<>(std::make_exception_ptr(
                gpu_exception("Wrong gpu device id!")));
        }
        return get_stream(device_id).then([device_id, func = std::move(gpu_func), args...](uint32_t stream_id) {
            cuda_set_device(device_id);
            auto& su = _streams[device_id][stream_id];
            func(su.stream, args...);
            cuda_event_handle event = create_cuda_event();
            cuda_stream_event_record(su.stream, event);
            if (++(su.count) < _event_num_per_stream) {
                _candidates[device_id]->push(stream_id);
            }
            auto pr_id = _pr_manager[device_id]->acquire_pr();
            _event_pr_pool[local_shard_id()][device_id].emplace(event, pr_id);
            return _pr_manager[device_id]->get_future(pr_id).then([pr_id, device_id, stream_id, &su] {
                _pr_manager[device_id]->remove_pr(pr_id);
                if ((su.count)-- == _event_num_per_stream) {
                    _candidates[device_id]->push(stream_id);
                }
                return seastar::make_ready_future<>();
            });
        });
    }

private:
    static void check_available_or_abort();
    static seastar::future<uint32_t> get_stream(uint32_t dev_id);

    static bool poll_queues();
    static bool pure_poll_queues();

    friend class gpu_resource_pollfn;
};

}  // namespace hiactor
