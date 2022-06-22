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

#include <queue>
#include <seastar/core/circular_buffer.hh>
#include <seastar/core/future.hh>
#include <seastar/util/std-compat.hh>

namespace hiactor {

/// Asynchronous single-producer single-consumer queue with limited capacity.
/// There can be at most one producer-side and at most one consumer-side operation active at any time.
/// Operations returning a future are considered to be active until the future resolves.
template <typename T>
class dynamic_queue {
    std::queue<T, seastar::circular_buffer<T>> _q;
    std::optional<seastar::promise<>> _not_empty;
    std::exception_ptr _ex = nullptr;
private:
    void notify_not_empty();
public:
    explicit dynamic_queue(size_t size);

    /// \brief Push an item.
    ///
    /// Returns false if the queue was full and the item was not pushed.
    bool push(T&& a);

    /// \brief Pop an item.
    ///
    /// Popping from an empty queue will result in undefined behavior.
    T pop();

    /// Consumes items from the queue, passing them to @func, until @func
    /// returns false or the queue it empty
    ///
    /// Returns false if func returned false.
    template <typename Func>
    bool consume(Func&& func);

    /// Returns true when the queue is empty.
    bool empty() const;

    /// Returns true when the queue is full.
    bool full() const;

    /// Returns a future<> that becomes available when pop() or consume()
    /// can be called.
    /// A consumer-side operation. Cannot be called concurrently with other consumer-side operations.
    seastar::future<> not_empty();

    /// Returns a future<> that becomes available when push() can be called.
    /// A producer-side operation. Cannot be called concurrently with other producer-side operations.
    seastar::future<> not_full();

    /// Pops element now or when there is some. Returns a future that becomes
    /// available when some element is available.
    /// If the queue is, or already was, abort()ed, the future resolves with
    /// the exception provided to abort().
    /// A consumer-side operation. Cannot be called concurrently with other consumer-side operations.
    seastar::future<T> pop_eventually();

    /// Pushes the element now or when there is room. Returns a future<> which
    /// resolves when data was pushed.
    /// If the queue is, or already was, abort()ed, the future resolves with
    /// the exception provided to abort().
    /// A producer-side operation. Cannot be called concurrently with other producer-side operations.
    seastar::future<> push_eventually(T&& data);

    /// Returns the number of items currently in the queue.
    size_t size() const { return _q.size(); }

    /// Destroy any items in the queue, and pass the provided exception to any
    /// waiting readers or writers - or to any later read or write attempts.
    void abort(std::exception_ptr ex) {
        while (!_q.empty()) {
            _q.pop();
        }
        _ex = ex;
        if (_not_empty) {
            _not_empty->set_exception(std::move(ex));
            _not_empty = std::nullopt;
        }
    }
};

template <typename T>
inline
dynamic_queue<T>::dynamic_queue(size_t size) {}

template <typename T>
inline
void dynamic_queue<T>::notify_not_empty() {
    if (_not_empty) {
        _not_empty->set_value();
        _not_empty = std::optional<seastar::promise<>>();
    }
}

template <typename T>
inline
bool dynamic_queue<T>::push(T&& data) {
    _q.push(std::move(data));
    notify_not_empty();
    return true;
}

template <typename T>
inline
T dynamic_queue<T>::pop() {
    T data = std::move(_q.front());
    _q.pop();
    return data;
}

template <typename T>
inline
seastar::future<T> dynamic_queue<T>::pop_eventually() {
    if (_ex) {
        return seastar::make_exception_future<T>(_ex);
    }
    if (empty()) {
        return not_empty().then([this] {
            if (_ex) {
                return seastar::make_exception_future<T>(_ex);
            } else {
                return seastar::make_ready_future<T>(pop());
            }
        });
    } else {
        return seastar::make_ready_future<T>(pop());
    }
}

template <typename T>
inline
seastar::future<> dynamic_queue<T>::push_eventually(T&& data) {
    if (_ex) {
        return seastar::make_exception_future<>(_ex);
    }
    _q.push(std::move(data));
    notify_not_empty();
    return seastar::make_ready_future<>();
}

template <typename T>
template <typename Func>
inline
bool dynamic_queue<T>::consume(Func&& func) {
    if (_ex) {
        std::rethrow_exception(_ex);
    }
    bool running = true;
    while (!_q.empty() && running) {
        running = func(std::move(_q.front()));
        _q.pop();
    }
    return running;
}

template <typename T>
inline
bool dynamic_queue<T>::empty() const {
    return _q.empty();
}

template <typename T>
inline
seastar::future<> dynamic_queue<T>::not_empty() {
    if (_ex) {
        return seastar::make_exception_future<>(_ex);
    }
    if (!empty()) {
        return seastar::make_ready_future<>();
    } else {
        _not_empty = seastar::promise<>();
        return _not_empty->get_future();
    }
}

} // namespace hiactor
