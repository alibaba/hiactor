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

#include <hiactor/core/coordinator_worker.hh>

#include <seastar/core/reactor.hh>

namespace hiactor {

coord_work_queue::coord_work_queue() : _pending(), _completed(), _start_eventfd(0) {}

void coord_work_queue::submit_item(std::unique_ptr<coord_work_queue::work_item> item) {
    _queue_has_room.wait().then([this, item = std::move(item)]() mutable {
        _pending.push(item.release());
        _start_eventfd.signal(1);
    });
}

unsigned coord_work_queue::complete() {
    std::array<work_item*, queue_length> tmp_buf{nullptr};
    auto end = tmp_buf.data();
    auto nr = _completed.consume_all([&](work_item* wi) {
        *end++ = wi;
    });
    for (auto p = tmp_buf.data(); p != end; ++p) {
        auto wi = *p;
        wi->complete();
        delete wi;
    }
    _queue_has_room.signal(nr);
    return nr;
}

coordinator_worker::coordinator_worker(seastar::reactor* r) : _reactor(r), _worker_thread([this] { work(); }) {}

void coordinator_worker::work() {
    pthread_setname_np(pthread_self(), "coordinator-worker");
    sigset_t mask;
    sigfillset(&mask);
    auto tr = ::pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    seastar::throw_pthread_error(tr);
    std::array<coord_work_queue::work_item*, coord_work_queue::queue_length> tmp_buf{nullptr};
    while (true) {
        uint64_t count;
        auto r = ::read(_inter_thread_wq._start_eventfd.get_read_fd(), &count, sizeof(count));
        assert(r == sizeof(count));
        if (_stopped.load(std::memory_order_relaxed)) {
            break;
        }
        auto end = tmp_buf.data();
        _inter_thread_wq._pending.consume_all([&](coord_work_queue::work_item* wi) {
            *end++ = wi;
        });
        for (auto p = tmp_buf.data(); p != end; ++p) {
            auto wi = *p;
            wi->process();
            _inter_thread_wq._completed.push(wi);
        }
        if (_main_thread_idle.load(std::memory_order_seq_cst)) {
            _reactor->wakeup();
        }
    }
}

coordinator_worker::~coordinator_worker() {
    _stopped.store(true, std::memory_order_relaxed);
    _inter_thread_wq._start_eventfd.signal(1);
    _worker_thread.join();
}

} // namespace hiactor
