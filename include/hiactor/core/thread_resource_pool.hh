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

#include <hiactor/core/shard-config.hh>

#include <boost/lockfree/queue.hpp>
#include <seastar/core/internal/pollable_fd.hh>
#include <seastar/util/noncopyable_function.hh>
#include <seastar/core/sleep.hh>
#include <seastar/util/std-compat.hh>

namespace hiactor {

class thread_resource_pool;

class work_thread {
    struct work_item {
        unsigned from_shard;

        explicit work_item(unsigned from_shard) : from_shard(from_shard) {}
        virtual ~work_item() = default;
        virtual void process() = 0;
        virtual void complete() = 0;
    };
    template <typename T>
    struct work_item_returning : work_item {
        seastar::noncopyable_function<T()> func;
        seastar::promise<T> promise;
        std::optional<T> result{};

        work_item_returning(unsigned from_shard, seastar::noncopyable_function<T()> func)
            : work_item(from_shard), func(std::move(func)) {}
        void process() override { result = this->func(); }
        void complete() override { promise.set_value(std::move(*result)); }
        seastar::future<T> get_future() { return promise.get_future(); }
    };
    std::unique_ptr<work_item> _work_item;
    seastar::writeable_eventfd _start_eventfd;
    seastar::posix_thread _worker;
    std::atomic<bool> _stopped{false};
    seastar::reactor* _employer = nullptr;
    unsigned _employer_shard = 0;
public:
    ~work_thread();
private:
    explicit work_thread(unsigned worker_id);

    template <typename T>
    seastar::future<T> submit(seastar::noncopyable_function<T()> func) {
        _employer = seastar::local_engine;
        _employer_shard = seastar::this_shard_id();
        auto* wi = new work_item_returning<T>(_employer_shard, std::move(func));
        auto fut = wi->get_future();
        _work_item.reset(wi);
        _start_eventfd.signal(1);
        return fut;
    }

    void work(unsigned worker_id);

    friend thread_resource_pool;
};

class thread_resource_pollfn;

class thread_resource_pool {
    static bool _active;
    static unsigned _worker_num;
    static int64_t _sleep_duration_in_microseconds;
    static std::unique_ptr<boost::lockfree::queue<work_thread*>> _threads;
    using completed_queue = boost::lockfree::queue<work_thread::work_item*>;
    struct completed_queue_deleter {
        void operator()(completed_queue* q) const;
    };
    static std::unique_ptr<completed_queue[], completed_queue_deleter> _completed;
    struct atomic_flag_deleter {
        void operator()(std::atomic<bool>* flags) const;
    };
    static std::unique_ptr<std::atomic<bool>[], atomic_flag_deleter> _reactors_idle;
    struct wt_pointer {
        work_thread* d;
        wt_pointer() : d(nullptr) {}
    };
public:
    static void configure(const boost::program_options::variables_map& configs);
    static void stop();
    static bool active();

    template <typename Func, typename Ret = typename std::result_of_t<Func()>>
    static seastar::future<Ret> submit_work(Func func) {
        if (!_active) {
            return seastar::make_exception_future<Ret>(std::make_exception_ptr(std::runtime_error(
                "Thread resource pool is not open. "
                "Try to add command line parameter \"--open-thread-resource-pool=true\".")));
        }
        return seastar::repeat_until_value([]() -> seastar::future<std::optional<work_thread*>> {
            work_thread* wt = nullptr;
            if (_threads->pop(wt)) {
                return seastar::make_ready_future<std::optional<work_thread*>>(wt);
            }
            return seastar::sleep(std::chrono::microseconds(_sleep_duration_in_microseconds)).then([] {
                return seastar::make_ready_future<std::optional<work_thread*>>(std::nullopt);
            });
        }).then([func = std::move(func)](work_thread* wt) {
            return wt->submit<Ret>(std::move(func));
        });
    }

private:
    static void complete_work_item(work_thread::work_item* wi);
    static void return_worker(work_thread* wt);

    static bool poll_queues();
    static bool pure_poll_queues();

    static void enter_interrupt_mode();
    static void exit_interrupt_mode();

    friend class work_thread;
    friend class thread_resource_pollfn;
};

} // namespace hiactor
