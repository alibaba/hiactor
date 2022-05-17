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

#include <hiactor/core/actor_message.hh>
#include <hiactor/core/local_channel.hh>

#include <string>
#include <vector>

namespace hiactor {

class actor_base;

typedef actor_base* (* ActorInstBuilder)(actor_base*, const byte_t*);

class actor_factory {
    std::vector<ActorInstBuilder> _actor_directory;
    actor_factory() = default;
public:
    static actor_factory& get();
    actor_base* create(uint16_t actor_tid, actor_base* exec_ctx, const byte_t* addr);
    void register_actor(uint16_t actor_tid, ActorInstBuilder func);
};

namespace registration {

template <typename T>
class actor_registration {
public:
    /// type id range(0, 61439)     : stateless/stateful actor
    /// type id range(61440, 65535) : actor group
    explicit actor_registration(uint16_t actor_tid) noexcept {
        static_assert(std::is_base_of<actor_base, T>::value, "T must be a derived class of actor_base.");
        actor_factory::get().register_actor(actor_tid, actor_builder_func);
    }
private:
    static actor_base* actor_builder_func(actor_base* exec_ctx, const byte_t* addr) {
        return new T{exec_ctx, addr};
    }
};

} // namespace registration

inline
actor_factory& actor_factory::get() {
    static actor_factory instance;
    return instance;
}

} // namespace hiactor
