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

#include <utility>

namespace hiactor {

template <typename T>
struct entry {
    union optional {
        optional() {}
        ~optional() {}
        template <typename... As>
        void init(As&& ... args) {
            new(&t) T{std::forward<As>(args)...};
        }
        T t;
    } o;
    entry() : _empty(true) {}
    ~entry() = default;

    template <typename... Args>
    void init(Args&& ... args);
    void finalize();

    inline bool empty() const {
        return _empty;
    }
private:
    bool _empty;
};

template <typename T>
struct entries_deleter {
    unsigned count;
    explicit entries_deleter(unsigned c = 0) : count(c) {}
    void operator()(T* es) const;
};

template <typename T>
template <typename... Args>
inline
void entry<T>::init(Args&& ... args) {
    if (_empty) {
        _empty = false;
        o.init(std::forward<Args>(args)...);
    }
}

template <typename T>
inline
void entry<T>::finalize() {
    if (!_empty) {
        _empty = true;
        o.t.~T();
    }
}

template <typename T>
inline
void entries_deleter<T>::operator()(T* es) const {
    for (unsigned i = 0; i < count; i++) {
        es[i].~T();
    }
    ::operator delete[](es);
}

} // namespace hiactor
