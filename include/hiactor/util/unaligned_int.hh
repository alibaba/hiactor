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

#include <cstring>
#include <type_traits>

namespace hiactor {

template <typename T>
inline
T load_unaligned_int(const void* ptr) {
    static_assert(std::is_integral<T>::value, "T must be an integer!");
    T ret;
    memcpy(&ret, ptr, sizeof(T));
    return ret;
}

template <typename T>
inline
T load_unaligned_int_partial(const void* ptr, size_t len) {
    static_assert(std::is_integral<T>::value, "T must be an integer with a length >= len!");
    T ret = 0;
    memcpy(&ret, ptr, len);
    return ret;
}

template <typename T>
inline
void store_unaligned_int(void* ptr, T const val) {
    static_assert(std::is_integral<T>::value, "T must be an integer!");
    memcpy(ptr, &val, sizeof(T));
}

} // namespace hiactor
