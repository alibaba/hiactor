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

#include <hiactor/core/gpu_resource_pool.hh>

#include <seastar/core/print.hh>
#include <seastar/core/sleep.hh>

namespace hiactor {

std::vector<std::vector<gpu_resource_pool::stream_unit> > gpu_resource_pool::_streams;
std::vector<std::unique_ptr<boost::lockfree::queue<uint32_t> > > gpu_resource_pool::_candidates;
std::vector<std::unique_ptr<pr_manager<> > > gpu_resource_pool::_pr_manager;
std::unique_ptr<gpu_resource_pool::event_pr_set*[], gpu_resource_pool::eps_deleter> gpu_resource_pool::_event_pr_pool;
uint32_t gpu_resource_pool::_device_num = 0;
uint32_t gpu_resource_pool::_stream_num_per_device = 4;
uint32_t gpu_resource_pool::_event_num_per_stream = 2;
int64_t gpu_resource_pool::_sleep_duration_in_microseconds = 500;

void gpu_resource_pool::eps_deleter::operator()(event_pr_set** eps) const {
    for (unsigned i = 0; i < local_shard_count(); i++) {
        for (unsigned j = 0; j < _device_num; j++) {
            eps[i][j].~event_pr_set();
        }
        ::operator delete[](eps[i]);
    }
    delete[](eps);
}

void gpu_resource_pool::configure(const boost::program_options::variables_map& configs) {
    // Configure variables
    if (configs.count("stream-num-per-gpu")) {
        _stream_num_per_device = configs["stream-num-per-gpu"].as<unsigned>();
    }
    if (configs.count("max-task-num-per-stream")) {
        _event_num_per_stream = configs["max-task-num-per-stream"].as<unsigned>();
    }

    _device_num = cuda_get_device_count();
    if (_device_num == 0) {
        throw gpu_exception("Failed to initialize gpu resource pool, gpu devices are not available!");
    }

    _streams.reserve(_device_num);
    _candidates.reserve(_device_num);
    _pr_manager.reserve(_device_num);
    for (uint32_t dev_id = 0; dev_id < _device_num; dev_id++) {
        cuda_set_device(dev_id);

        // Create initial streams
        std::vector<stream_unit> stream_vec;
        stream_vec.reserve(_stream_num_per_device);
        for (uint32_t i = 0; i < _stream_num_per_device; i++) {
            stream_vec.emplace_back(create_cuda_stream(), 0);
        }
        _streams.push_back(std::move(stream_vec));

        // Create stream candidate queues
        auto candidate_queue = std::make_unique<boost::lockfree::queue<uint32_t> >(_stream_num_per_device);
        for (uint32_t i = 0; i < _stream_num_per_device; i++) {
            candidate_queue->push(i);
        }
        _candidates.push_back(std::move(candidate_queue));

        // Create event promise manager
        _pr_manager.push_back(std::make_unique<pr_manager<> >(_stream_num_per_device * _event_num_per_stream));
    }

    // Create event promise pool
    _event_pr_pool = decltype(gpu_resource_pool::_event_pr_pool){
        new event_pr_set* [local_shard_count()], eps_deleter{}};
    for (uint32_t i = 0; i < local_shard_count(); i++) {
        _event_pr_pool[i] = reinterpret_cast<event_pr_set*>(operator new[] (sizeof(event_pr_set) * _device_num));
        for (uint32_t j = 0; j < _device_num; j++) {
            new (&gpu_resource_pool::_event_pr_pool[i][j]) event_pr_set();
        }
    }
}

void gpu_resource_pool::stop() {
    for (uint32_t i = 0; i < local_shard_count(); i++) {
        for (uint32_t j = 0; j < _device_num; j++) {
            assert(_event_pr_pool[i][j].empty());
        }
    }
}

void gpu_resource_pool::check_available_or_abort() {
#ifndef HIACTOR_GPU_ENABLE
    fmt::print("GPU resource pool is disable now. "
               "Try to add cmake option \"-DHiactor_GPU_ENABLE=ON\" "
               "or add command line parameter \"--open-gpu-resource-pool\" "
               "when configuring.\n");
    abort();
#endif
}

seastar::future<uint32_t> gpu_resource_pool::get_stream(uint32_t dev_id) {
    uint32_t stream_id = 0;
    return seastar::now().then([&stream_id, dev_id] {
        if (!_candidates[dev_id]->pop(stream_id)) {
            return seastar::repeat([&stream_id, dev_id] {
                return seastar::sleep(std::chrono::microseconds(_sleep_duration_in_microseconds)).then([&stream_id, dev_id] {
                    return _candidates[dev_id]->pop(stream_id) ? seastar::stop_iteration::yes : seastar::stop_iteration::no;
                });
            });
        }
        return seastar::make_ready_future<>();
    }).then([&stream_id] {
        return seastar::make_ready_future<uint32_t>(stream_id);
    });
}

bool gpu_resource_pool::poll_queues() {
    size_t got = 0;
    for (uint32_t dev_id = 0; dev_id < _device_num; dev_id++) {
        auto it = _event_pr_pool[local_shard_id()][dev_id].begin();
        while (it != _event_pr_pool[local_shard_id()][dev_id].end()) {
            if (cuda_check((*it).event)) {
                _pr_manager[dev_id]->set_value((*it).pr_id);
                it = _event_pr_pool[local_shard_id()][dev_id].erase(it);
                got++;
                continue;
            }
            it++;
        }
    }
    return got != 0;
}

bool gpu_resource_pool::pure_poll_queues() {
    for (uint32_t dev_id = 0; dev_id < _device_num; dev_id++) {
        if (!_event_pr_pool[local_shard_id()][dev_id].empty()) {
            return true;
        }
    }
    return false;
}

}  // namespace hiactor
