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
#include <hiactor/core/actor_timer.hh>
#include <hiactor/core/reference_base.hh>

#include <seastar/util/bool_class.hh>

namespace hiactor {

enum schedule_status : uint8_t {
    INITIALIZED,
    SCHEDULABLE,
    SCHEDULED
};

enum actor_status : uint8_t {
    RUNNING,
    PEACE_STOPPING,
    FORCE_STOPPING
};

struct stop_reaction_tag {};
using stop_reaction = seastar::bool_class<stop_reaction_tag>;

class actor_factory;
class root_actor_group;
class actor_group;
template <uint32_t> class reentrant_actor;

class actor_base : public seastar::execution_context, public seastar::task {
    clock_unit _unused_quota{};
    std::chrono::steady_clock::time_point _stop_time;
    schedule_status _sched_status = schedule_status::INITIALIZED;
    actor_status _stop_status = actor_status::RUNNING;
    uint8_t _total_addr_length;
    byte_t _address[GLocalActorAddrLength];
    seastar::circular_buffer<actor_message*> _mailbox; // NOT thread-safe queue
    std::unordered_map<std::string, boost::any> _meta_map;
private:
    virtual actor_base* get_actor_local(actor_message::header& hdr);
    uint8_t get_address_length() const;
    void log_exception(std::exception_ptr eptr);
    bool activatable();
    void set_activatable();
    void activate();
    void schedule();
    void enque_message(actor_message* msg);
    void enque_urgent_message(actor_message* msg);
    actor_message* deque_message();
    void set_timer();
    bool need_yield();
    bool stopping();
    bool force_stopping();
    void clean_mailbox();
    virtual void notify_child_stopped() {};
    virtual void stop_child_actor(byte_t* child_addr, bool force) {}
    virtual void stop(bool force) {}

    friend class actor_factory;
    friend class root_actor_group;
    friend class actor_group;
    template <uint32_t> friend class reentrant_actor;
public:
    explicit actor_base(actor_base* exec_ctx, const byte_t* addr);
    explicit actor_base(actor_base* exec_ctx);
    virtual ~actor_base();

    /// FIXME: No use in hiactor
    task* waiting_task() noexcept override { return nullptr; }

    scope_builder get_scope();

    uint16_t actor_type_id() const { return load_unaligned_int<uint16_t>(_address); }
    uint32_t actor_id() const { return load_unaligned_int<uint32_t>(_address + GActorTypeInBytes); }

    /// Whether this actor contains a meta with param \key.
    bool contains_meta(const std::string& key) const { return _meta_map.count(key) > 0; }

    /// Get meta type name of param \key.
    /// May throw `std::out_of_range` exception if \key is not existed.
    const char* meta_type_name(const std::string& key) const { return _meta_map.at(key).type().name(); }

    /// Add user meta data into an actor.
    template <typename T>
    void add_meta(const std::string& key, const T& value) { _meta_map[key] = value; }

    /// Get the user meta data copy or pointer of param \key.
    ///
    /// May throw `std::out_of_range` exception if \key is not existed or
    /// throw `boost::bad_any_cast` exception if meta type T mismatches,
    /// user should ensure the correctness here.
    template <typename T>
    T get_meta(const std::string& key) const { return boost::any_cast<T>(_meta_map.at(key)); }
    template <typename T>
    const T* get_meta_ptr(const std::string& key) const { return boost::any_cast<T>(&_meta_map.at(key)); }
};

inline
uint8_t actor_base::get_address_length() const {
    return _total_addr_length;
}

inline
bool actor_base::activatable() {
    return _sched_status == schedule_status::SCHEDULABLE;
}

inline
void actor_base::set_activatable() {
    _sched_status = schedule_status::SCHEDULABLE;
}

inline
void actor_base::schedule() {
    assert(_sched_status == schedule_status::INITIALIZED);
    set_activatable();
    activate();
}

inline
bool actor_base::stopping() {
    return _stop_status != actor_status::RUNNING;
}

inline
bool actor_base::force_stopping() {
    return _stop_status == actor_status::FORCE_STOPPING;
}

} // namespace hiactor
