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

#include <cstdint>
#include <cstddef>

#define ANNOTATION(NAME) __attribute__((annotate(#NAME)))
#define ACTOR_DO_WORK() seastar::future<hiactor::stop_reaction> do_work(hiactor::actor_message* msg) override;

namespace hiactor {

using byte_t = uint8_t;

template <typename T, typename U>
inline
constexpr size_t offset_of(U T::*member) {
    T* obj_ptr = nullptr;
    return (char*) &(obj_ptr->*member) - (char*) obj_ptr;
}

inline
unsigned* as_u32_ptr(void* data) {
    return reinterpret_cast<uint32_t*>(data);
}

inline
unsigned* to_u32_ptr(void* data) {
    return static_cast<uint32_t*>(data);
}

inline
byte_t* as_byte_ptr(void* data) {
    return reinterpret_cast<byte_t*>(data);
}

inline
byte_t* to_byte_ptr(void* data) {
    return static_cast<byte_t*>(data);
}

} // namespace hiactor
