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

#include <hiactor/core/actor_factory.hh>
#include <hiactor/core/actor_message.hh>
#include <hiactor/util/machine_info.hh>

namespace hiactor {

class scope_builder;

class reference_base {
private:
    void make_address(address& src_addr, uint32_t actor_id) {
        memcpy(addr.data, src_addr.data, src_addr.length);
        addr.length = src_addr.length;

        auto offset = addr.data + addr.length;
        memcpy(offset, &actor_type, GActorTypeInBytes);
        offset += GActorTypeInBytes;
        memcpy(offset, &actor_id, GActorIdInBytes);
        addr.length += GLocalActorAddrLength;
    }
protected:
    int16_t actor_type = 0;
    address addr;
    reference_base() = default;
    friend class scope_builder;
public:
    virtual ~reference_base() = default;
};

/// Get the actor group type id.
///
/// Hiactor codegen tool will generate different \get_actor_group_type_id impls
/// for user defined actor groups derived from \actor_group.
template <class ActorGroupType>
uint16_t get_actor_group_type_id();

template <typename T>
struct scope {
private:
    uint32_t scope_id;
public:
    explicit scope<T>(uint32_t id) { scope_id = id; }

    uint16_t get_type() {
        return get_actor_group_type_id<T>();
    }

    uint32_t get_id() {
        return scope_id;
    }
};

class scope_builder {
    address addr;
    friend class actor_base;
    friend class root_actor_group;
public:
    scope_builder() {
        set_shard(0);
        addr.length = GShardIdInBytes;
    }

    explicit scope_builder(uint32_t shard_id) {
        set_shard(shard_id);
        addr.length = GShardIdInBytes;
    }

    template <typename... Scopes>
    explicit scope_builder(uint32_t shard_id, Scopes ... scopes) {
        assert(sizeof...(Scopes) < GMaxAddrLayer);
        set_shard(shard_id);
        addr.length = GShardIdInBytes;

        auto scope_list = std::forward_as_tuple(scopes...);
        foreach_init_scope(scope_list);
    }

    template <typename T>
    scope_builder& enter_sub_scope(scope<T> sub) {
        assert(addr.length + GLocalActorAddrLength < GMaxAddrLength);
        uint16_t actor_group_type = sub.get_type();
        memcpy(addr.data + addr.length, &actor_group_type, GActorTypeInBytes);
        uint32_t actor_group_id = sub.get_id();
        memcpy(addr.data + addr.length + GActorTypeInBytes, &actor_group_id, GActorIdInBytes);
        addr.length += GLocalActorAddrLength;
        return *this;
    }

    scope_builder& back_to_parent_scope() {
        assert(addr.length >= (GShardIdInBytes + GLocalActorAddrLength));
        addr.length -= GLocalActorAddrLength;
        return *this;
    }

    uint32_t current_scope_id() const {
        assert(addr.length >= (GShardIdInBytes + GLocalActorAddrLength));
        return *reinterpret_cast<const uint32_t*>(addr.data + addr.length - GActorIdInBytes);
    }

    size_t scope_layer_number() const {
        return ((addr.length - GShardIdInBytes) / GLocalActorAddrLength);
    }

    scope_builder& set_shard(uint32_t shard_id) {
        memcpy(addr.data, &shard_id, GShardIdInBytes);
        return *this;
    }

    uint32_t shard() const {
        return *reinterpret_cast<const uint32_t*>(addr.data);
    }

    template <typename T>
    std::enable_if_t<(std::is_base_of_v<reference_base, T> && std::is_default_constructible_v<T>), T>
    build_ref(uint32_t actor_id) {
        T reference;
        reference.make_address(addr, actor_id);
        return reference;
    }

    template <typename T>
    std::enable_if_t<(std::is_base_of_v<reference_base, T> && std::is_default_constructible_v<T>), T*>
    new_ref(uint32_t actor_id) {
        auto* p_ref = new T();
        p_ref->make_address(addr, actor_id);
        return p_ref;
    }

private:
    template <size_t I = 0, typename... Scopes>
    std::enable_if_t<I == sizeof...(Scopes), void>
    foreach_init_scope(const std::tuple<Scopes...>& scopes, size_t layer = I) {}

    template <size_t I = 0, typename... Scopes>
    std::enable_if_t<I < sizeof...(Scopes), void>
    foreach_init_scope(const std::tuple<Scopes...>& scopes, size_t layer = I) {
        auto offset = addr.data + addr.length;
        uint16_t actor_group_type = std::get<I>(scopes).get_type();
        memcpy(offset, &actor_group_type, GActorTypeInBytes);
        offset += GActorTypeInBytes;
        uint32_t actor_group_id = std::get<I>(scopes).get_id();
        memcpy(offset, &actor_group_id, GActorIdInBytes);
        addr.length += GLocalActorAddrLength;
        foreach_init_scope<I + 1>(scopes, layer + 1);
    }
};

} // namespace hiactor
