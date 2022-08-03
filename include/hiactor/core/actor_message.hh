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

#include <hiactor/net/serializable_queue.hh>
#include <hiactor/util/common-utils.hh>
#include <hiactor/util/machine_info.hh>
#include <hiactor/util/unaligned_int.hh>

#include <cstring>
#include <string>
#include <boost/range/irange.hpp>
#include <seastar/core/iostream.hh>
#include <seastar/core/temporary_buffer.hh>

namespace hiactor {

const size_t GActorTypeInBytes = 2;
const size_t GShardIdInBytes = 4;
const size_t GActorIdInBytes = 4;
const size_t GMaxAddrLayer = 8;
const size_t GLocalActorAddrLength = GActorTypeInBytes + GActorIdInBytes; // 6 bytes
const size_t GMaxAddrLength = GShardIdInBytes + GMaxAddrLayer * GLocalActorAddrLength; // 52 bytes

enum message_type : uint8_t {
    USER = 0x01,
    RESPONSE = 0x02,
    EXCEPTION_RESPONSE = 0x03,
    FORCE_STOP = 0x04,
    PEACE_STOP = 0x08,
};

/***
 * Format: shard_id, actor_group_id_1, ..., actor_group_id_6, actor_id
 * shard_id: (4 bytes)
 * actor_group_id_x: class_type (2 bytes) + group_guid (4 bytes)
 * actor_id: class_type (2 bytes) + actor_guid (4 bytes)
 */
// TODO: Refactor
struct address {
    byte_t length = 0;
    byte_t data[GMaxAddrLength];
    address() = default;
    address(const address&) = default;
    explicit address(uint32_t shard_id) {
        memcpy(data, &shard_id, GShardIdInBytes);
        length = GShardIdInBytes;
    }

    inline uint32_t get_shard_id() const {
        return load_unaligned_int<uint32_t>(data);
    }

    /// Set/Get the actor type id to which the target actor method really belongs.
    ///
    /// In Hiactor, we need to support polymorphism and allow users to define inheritance relationships
    /// between their customized actors. If a derived actor B does not override a virtual method `func`
    /// defined in its parent actor A, calling `func` from B will set this field with A's actor type id.
    /// When actor message is processed in B's `do_work`, this field can be used to distinguish whose
    /// polymorphic method should be called between A and B.
    ///
    /// The method actor type is stored in the tail of `addr`.
    inline void set_method_actor_tid(uint16_t method_actor_type) {
        auto* offset = data + GMaxAddrLength - GActorTypeInBytes;
        memcpy(offset, &method_actor_type, GActorTypeInBytes);
    }
    inline uint16_t get_method_actor_tid() const {
        auto* offset = data + GMaxAddrLength - GActorTypeInBytes;
        return load_unaligned_int<uint16_t>(offset);
    }
};

struct network_delegater;
struct actor_message_without_payload;
template <typename T>
struct actor_message_with_payload;

struct actor_message {
    struct network_flag {};

    struct header {
        // actor address
        address addr;
        // behavior type id
        uint8_t behavior_tid;
        // fields used to address response
        uint32_t src_shard_id;
        uint32_t pr_id;

        message_type m_type;
        bool from_network;
        // to make header a POD type, we explicitly define a default constructor
        header() = default;

        header(const address& addr, uint8_t behv_tid, uint32_t src_s_id,
               uint32_t pr_id, message_type m_type, bool from_network)
            : addr(addr), behavior_tid(behv_tid), src_shard_id(src_s_id),
              pr_id(pr_id), m_type(m_type), from_network(from_network) {}

        header(const header& x) = default;
    } hdr;

    actor_message(const actor_message& x) = default;
    explicit actor_message(const header& hdr) : hdr(hdr) {}
    virtual ~actor_message() = default;
    virtual seastar::future<> serialize(seastar::output_stream<char>& out) = 0;

private:
    // Local
    actor_message(const address& addr, uint8_t behv_tid, uint32_t src_s_id, uint32_t pr_id, message_type m_type)
        : hdr(addr, behv_tid, src_s_id, pr_id, m_type, false) {}

    // Remote
    actor_message(const address& addr, uint8_t behv_tid, uint32_t src_s_id,
                  uint32_t pr_id, message_type m_type, network_flag)
        : hdr(addr, behv_tid, src_s_id, pr_id, m_type, true) {}
    // static_assert(std::is_pod<header>::value, "header type should be pod.");

    friend struct actor_message_without_payload;
    template <typename T>
    friend struct actor_message_with_payload;
};

struct actor_message_without_payload final : public actor_message {
    actor_message_without_payload(const actor_message_without_payload&) = delete;
    actor_message_without_payload(actor_message_without_payload&& x) = delete;
    ~actor_message_without_payload() override = default;

    static actor_message_without_payload*
    make_actor_message(const address& addr, uint8_t behv_tid, uint32_t src_s_id, uint32_t pr_id, message_type m_type) {
        return new actor_message_without_payload{
            addr, behv_tid, src_s_id, pr_id, m_type
        };
    }

    seastar::future<> serialize(seastar::output_stream<char>& out) final {
        static const uint32_t nr_payload = 0;
        static constexpr uint32_t msg_len = sizeof(header) + 4;
        return out.write(reinterpret_cast<const char*>(&msg_len), 4).then([this, &out] {
            return out.write(reinterpret_cast<char*>(&hdr), sizeof(header));
        }).then([&out] {
            return out.write(reinterpret_cast<const char*>(&nr_payload), 4);
        });
    }

private:
    actor_message_without_payload(const address& addr, uint8_t behv_tid, uint32_t src_s_id,
                                  uint32_t pr_id, message_type m_type)
        : actor_message(addr, behv_tid, src_s_id, pr_id, m_type) {}

    // Can only be called by network_delegater.
    actor_message_without_payload(const address& addr, uint8_t behv_tid, uint32_t src_s_id,
                                  uint32_t pr_id, message_type m_type, network_flag)
        : actor_message(addr, behv_tid, src_s_id, pr_id, m_type, network_flag{}) {}

    friend struct network_delegater;
};

template <typename T>
struct actor_message_with_payload final : public actor_message {
    T data;
    actor_message_with_payload(const actor_message_with_payload&) = delete;
    actor_message_with_payload(actor_message_with_payload&&) = delete;
    ~actor_message_with_payload() override = default;

    static actor_message_with_payload*
    make_actor_message(const address& addr, uint8_t behv_tid, T&& data, uint32_t src_s_id,
                       uint32_t pr_id, message_type m_type) {
        static_assert(std::is_rvalue_reference<T&&>::value, "T&& should be rvalue reference.");
        return new actor_message_with_payload{
            addr, behv_tid, std::forward<T>(data), src_s_id, pr_id, m_type};
    }

    seastar::future<> serialize(seastar::output_stream<char>& out) final {
        return seastar::do_with(serializable_queue(), [&out, this](serializable_queue& su) {
            data.dump_to(su);
            uint32_t msg_len = sizeof(header) + 4 + su.size() * 4;
            msg_len += su.get_buffer_size();

            return out.write(reinterpret_cast<const char*>(&msg_len), 4).then([this, &out] {
                return out.write(reinterpret_cast<char*>(&hdr), sizeof(header));
            }).then([&out, &su] {
                auto size = static_cast<uint32_t>(su.size());
                return out.write(reinterpret_cast<const char*>(&size), 4);
            }).then([&out, &su] {
                return seastar::do_until([&su] { return su.empty(); }, [&out, &su]() {
                    auto buf = su.pop();
                    auto size = static_cast<uint32_t>(buf.size());
                    return out.write(reinterpret_cast<char*>(&size), 4).then(
                        [&out, buf = std::move(buf)] {
                            return out.write(buf.get(), buf.size());
                        });
                });
            });
        });
    }

private:
    actor_message_with_payload(const address& addr, uint8_t behv_tid, T&& data, uint32_t src_s_id,
                               uint32_t pr_id, message_type m_type)
        : actor_message(addr, behv_tid, src_s_id, pr_id, m_type), data(std::forward<T>(data)) {}
};

template <>
struct actor_message_with_payload<serializable_queue> final : public actor_message {
    serializable_queue data;
    actor_message_with_payload(const actor_message_with_payload& other) = delete;
    actor_message_with_payload(actor_message_with_payload&& x) = delete;
    ~actor_message_with_payload() override = default;

    seastar::future<> serialize(seastar::output_stream<char>& out) final {
        // deprecated & useless.
        return seastar::make_ready_future<>();
    }

private:
    actor_message_with_payload(const address& addr, uint8_t behv_tid, serializable_queue&& data,
                               uint32_t src_s_id, uint32_t pr_id, message_type m_type)
        : actor_message(addr, behv_tid, src_s_id, pr_id, m_type, network_flag{}), data(std::move(data)) {}

    friend struct network_delegater;
};

inline
actor_message_without_payload* make_system_message(address& addr, message_type m_type) {
    return actor_message_without_payload::make_actor_message(addr, 0, 0, 0, m_type);
}

inline
actor_message_without_payload* make_request_message(const address& addr, uint8_t behv_tid, uint32_t src_s_id,
                                                    uint32_t pr_id, message_type m_type) {
    return actor_message_without_payload::make_actor_message(addr, behv_tid, src_s_id, pr_id, m_type);
}

inline
actor_message_without_payload* make_one_way_request_message(const address& addr, uint8_t behv_tid,
                                                            uint32_t src_s_id, message_type m_type) {
    return actor_message_without_payload::make_actor_message(addr, behv_tid, src_s_id, 0, m_type);
}

template <typename T, typename = typename std::enable_if<std::is_rvalue_reference<T&&>::value>::type>
inline
actor_message_with_payload<T>* make_request_message(const address& addr, uint8_t behv_tid, T&& data,
                                                    uint32_t src_s_id, uint32_t pr_id, message_type m_type) {
    return actor_message_with_payload<T>::make_actor_message(
        addr, behv_tid, std::forward<T>(data), src_s_id, pr_id, m_type);
}

template <typename T, typename = typename std::enable_if<std::is_rvalue_reference<T&&>::value>::type>
inline
actor_message_with_payload<T>* make_one_way_request_message(const address& addr, uint8_t behv_tid, T&& data,
                                                            uint32_t src_s_id, message_type m_type) {
    return actor_message_with_payload<T>::make_actor_message(
        addr, behv_tid, std::forward<T>(data), src_s_id, 0, m_type);
}

inline
actor_message_without_payload*
make_response_message(uint32_t s_id, uint32_t pr_id, message_type m_type) {
    address addr{};
    addr.length = GShardIdInBytes;
    memcpy(addr.data, &s_id, GShardIdInBytes);
    return actor_message_without_payload::make_actor_message(addr, 0, 0, pr_id, m_type);
}

template <typename T, typename = typename std::enable_if<std::is_rvalue_reference<T&&>::value>::type>
inline
actor_message_with_payload<T>*
make_response_message(uint32_t s_id, T&& data, uint32_t pr_id, message_type m_type) {
    address addr{};
    addr.length = GShardIdInBytes;
    memcpy(addr.data, &s_id, GShardIdInBytes);
    return actor_message_with_payload<T>::make_actor_message(addr, 0, std::forward<T>(data), 0, pr_id, m_type);
}

std::string char_arr_to_str(const byte_t* addr, int len);

} // namespace hiactor
