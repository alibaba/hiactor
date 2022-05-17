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
#include <hiactor/core/message_tag.hh>
#include <hiactor/core/shard-config.hh>
#include <hiactor/util/machine_info.hh>

#include <unistd.h>
#include <memory>
#include <boost/lockfree/spsc_queue.hpp>
#include <seastar/core/cacheline.hh>
#include <seastar/core/metrics_registration.hh>

namespace hiactor {

class actor_msg_service_group;
struct actor_msg_service_group_config;

namespace internal {

unsigned actor_msg_service_group_id(actor_msg_service_group ssg);

}

/// Returns the default actor_msg_service_group. This actor_msg_service_group
/// does not impose any limits on concurrency in the target shard.
/// This makes is deadlock-safe, but can consume unbounded resources,
/// and should therefore only be used when initiator concurrency is
/// very low (e.g. administrative tasks).
actor_msg_service_group default_actor_msg_service_group();

/// Creates an actor_msg_service_group with the specified configuration.
///
/// The actor_msg_service_group is global, and after this call completes,
/// the returned value can be used on any shard.
seastar::future<actor_msg_service_group> create_actor_msg_service_group(actor_msg_service_group_config ssgc);

/// Destroy an actor_msg_service_group.
///
/// Frees all resources used by an actor_msg_service_group. It must not
/// be used again once this function is called.
seastar::future<> destroy_actor_msg_service_group(actor_msg_service_group ssg);

/// A resource controller for cross-shard calls.
///
/// An actor_msg_service_group allows you to limit the concurrency of
/// smp::send_to() and similar calls. While it's easy to limit
/// the caller's concurrency (for example, by using a semaphore),
/// the concurrency at the remote end can be multiplied by a factor
/// of smp::count-1, which can be large.
///
/// The class is called a service _group_ because it can be used
/// to group similar calls that share resource usage characteristics,
/// need not be isolated from each other, but do need to be isolated
/// from other groups. Calls in a group should not nest; doing so
/// can result in ABA deadlocks.
///
/// Nested send_to() calls must form a directed acyclic graph
/// when considering their actor_msg_service_groups as nodes. For example,
/// if a call using ssg1 then invokes another call using ssg2, the
/// internal call may not call again via either ssg1 or ssg2, or it
/// may form a cycle (and risking an ABBA deadlock). Create a
/// new actor_msg_service_group_instead.
class actor_msg_service_group {
    unsigned _id;
private:
    explicit actor_msg_service_group(unsigned id) : _id(id) {}

    friend unsigned internal::actor_msg_service_group_id(actor_msg_service_group ssg);
    friend actor_msg_service_group default_actor_msg_service_group();
    friend seastar::future<actor_msg_service_group> create_actor_msg_service_group(actor_msg_service_group_config ssgc);
};

inline
actor_msg_service_group default_actor_msg_service_group() {
    return actor_msg_service_group(0);
}

inline
unsigned internal::actor_msg_service_group_id(actor_msg_service_group ssg) {
    return ssg._id;
}

void init_default_actor_msg_service_group();

/// Configuration for actor_msg_service_group objects.
///
/// \see create_actor_msg_service_group()
struct actor_msg_service_group_config {
    /// The maximum number of non-local requests that execute on a shard concurrently
    ///
    /// Will be adjusted upwards to allow at least one request per non-local shard.
    unsigned max_nonlocal_requests = 0;
};

class root_actor_group;

class actor_message_queue final {
    static constexpr size_t queue_length = 128;
    static constexpr size_t batch_size = 16;
    static constexpr size_t prefetch_cnt = 2;
    struct lf_queue_remote {
        root_actor_group* remote;
    };
    using lf_queue_base = boost::lockfree::spsc_queue<actor_message*, boost::lockfree::capacity<queue_length>>;
    // use inheritence to control placement order
    struct lf_queue : lf_queue_remote, lf_queue_base {
        explicit lf_queue(root_actor_group* remote) : lf_queue_remote{remote} {}
        void maybe_wakeup();
    };
    lf_queue _pending;
    lf_queue _completed;
    struct alignas(seastar::cache_line_size) {
        size_t _sent = 0;
        size_t _compl = 0;
        size_t _last_snt_batch = 0;
        size_t _last_cmpl_batch = 0;
        size_t _current_queue_length = 0;
    };
    // keep this between two structures with statistics
    // this makes sure that they have at least one cache line
    // between them, so hw prefetcher will not accidentally prefetch
    // cache line used by another cpu.
    seastar::metrics::metric_groups _metrics;
    struct alignas(seastar::cache_line_size) {
        size_t _received = 0;
        size_t _last_rcv_batch = 0;
    };
    union tx_side {
        tx_side() {}
        ~tx_side() {}
        void init() { new(&a) aa; }
        struct aa {
            std::deque<actor_message*> pending_fifo;
        } a;
    } _tx;
public:
    actor_message_queue(root_actor_group* from, root_actor_group* to);
    ~actor_message_queue();
    seastar::future<> send(seastar::shard_id t, actor_msg_service_group ssg, actor_message* msg);
    seastar::future<> send(seastar::shard_id t, actor_message* msg);
    seastar::future<> send_urgent(seastar::shard_id t, actor_message* msg);
    void start(unsigned cpuid);
    size_t process_incoming();
    void stop();
private:
    void move_pending();
    void flush_request_batch();
    bool pure_poll_rx() const;
    friend class local_channel;
};

class actor_smp;
class actor_smp_pollfn;

class local_channel {
    struct qs_deleter {
        unsigned count;
        explicit qs_deleter(unsigned n = 0) : count(n) {}
        qs_deleter(const qs_deleter& d) = default;
        void operator()(actor_message_queue** qs) const;
    };
    using qs = std::unique_ptr<actor_message_queue* [], qs_deleter>;
public:
    template <typename Tag = normal_message_tag>
    static seastar::future<> send(actor_msg_service_group ssg, actor_message* msg) {
        static_assert(std::is_same<Tag, urgent_message_tag>::value, "Urgent message doesn't require semaphores.");
        return send_helper(ssg, msg, Tag{});
    }

    template <typename Tag = normal_message_tag>
    static seastar::future<> send(actor_message* msg) {
        return send_helper(msg, Tag{});
    }
private:
    static void create_qs(const std::vector<root_actor_group*>& root_actor_groups);
    static bool poll_queues();
    static bool pure_poll_queues();

    static seastar::future<> send_helper(actor_msg_service_group ssg, actor_message* msg, normal_message_tag) {
        auto target_cpu = msg->hdr.addr.get_shard_id() - machine_info::sid_anchor();
        return _qs[target_cpu][local_shard_id()].send(target_cpu, ssg, msg);
    }

    static seastar::future<> send_helper(actor_message* msg, normal_message_tag) {
        auto target_cpu = msg->hdr.addr.get_shard_id() - machine_info::sid_anchor();
        return _qs[target_cpu][local_shard_id()].send(target_cpu, msg);
    }

    static seastar::future<> send_helper(actor_message* msg, urgent_message_tag) {
        auto target_cpu = msg->hdr.addr.get_shard_id() - machine_info::sid_anchor();
        return _qs[target_cpu][local_shard_id()].send_urgent(target_cpu, msg);
    }

    static unsigned count;
    static qs _qs;

    friend class actor_smp;
    friend class actor_smp_pollfn;
    friend class root_actor_group;
};

} // namespace hiactor
