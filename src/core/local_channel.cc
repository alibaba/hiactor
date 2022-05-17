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

#include <hiactor/core/local_channel.hh>
#include <hiactor/core/root_actor_group.hh>
#include <hiactor/util/unaligned_int.hh>

#include <boost/range/algorithm/find_if.hpp>
#include <seastar/core/prefetch.hh>

namespace hiactor {

struct actor_msg_service_group_impl {
    std::vector<seastar::semaphore> clients;   // one client per server shard
};

static seastar::semaphore actor_msg_service_group_management_sem{1};
static thread_local std::vector<actor_msg_service_group_impl> actor_msg_service_groups;

seastar::future<actor_msg_service_group> create_actor_msg_service_group(actor_msg_service_group_config ssgc) {
    ssgc.max_nonlocal_requests = std::max(ssgc.max_nonlocal_requests, seastar::smp::count - 1);
    return seastar::smp::submit_to(0, [ssgc] {
        return seastar::with_semaphore(actor_msg_service_group_management_sem, 1, [ssgc] {
            auto it = boost::range::find_if(
                actor_msg_service_groups, [&](actor_msg_service_group_impl& ssgi) { return ssgi.clients.empty(); });
            size_t id = it - actor_msg_service_groups.begin();
            return seastar::smp::invoke_on_all([ssgc, id] {
                if (id >= actor_msg_service_groups.size()) {
                    actor_msg_service_groups.resize(id + 1); // may throw
                }
                actor_msg_service_groups[id].clients.reserve(seastar::smp::count); // may throw
                auto per_client = seastar::smp::count > 1 ? ssgc.max_nonlocal_requests / (seastar::smp::count - 1) : 0u;
                for (unsigned i = 0; i != seastar::smp::count; ++i) {
                    actor_msg_service_groups[id].clients.emplace_back(per_client);
                }
            }).handle_exception([id](std::exception_ptr e) {
                // rollback
                return seastar::smp::invoke_on_all([id] {
                    if (actor_msg_service_groups.size() > id) {
                        actor_msg_service_groups[id].clients.clear();
                    }
                }).then([e = std::move(e)]() mutable {
                    std::rethrow_exception(std::move(e));
                });
            }).then([id] {
                return seastar::make_ready_future<actor_msg_service_group>(actor_msg_service_group(id));
            });
        });
    });
}

seastar::future<> destroy_actor_msg_service_group(actor_msg_service_group ssg) {
    return seastar::smp::submit_to(0, [ssg] {
        return seastar::with_semaphore(actor_msg_service_group_management_sem, 1, [ssg] {
            auto id = internal::actor_msg_service_group_id(ssg);
            return seastar::smp::invoke_on_all([id] {
                actor_msg_service_groups[id].clients.clear();
            });
        });
    });
}

void init_default_actor_msg_service_group() {
    actor_msg_service_groups.emplace_back();
    auto& ssg0 = actor_msg_service_groups.back();
    ssg0.clients.reserve(seastar::smp::count);
    for (unsigned i = 0; i != seastar::smp::count; ++i) {
        ssg0.clients.emplace_back(seastar::semaphore::max_counter());
    }
}

void actor_message_queue::lf_queue::maybe_wakeup() {
    // Called after lf_queue_base::push().
    //
    // This is read-after-write, which wants memory_order_seq_cst,
    // but we insert that barrier using systemwide_memory_barrier()
    // because seq_cst is so expensive.
    //
    // However, we do need a compiler barrier:
    std::atomic_signal_fence(std::memory_order_seq_cst);
    if (remote->_sleeping.load(std::memory_order_relaxed)) {
        remote->_sleeping.store(false, std::memory_order_relaxed);
        remote->wakeup_reactor();
    }
}

actor_message_queue::actor_message_queue(root_actor_group* from, root_actor_group* to) : _pending(to), _completed(from) {}

actor_message_queue::~actor_message_queue() {
    if (_pending.remote != _completed.remote) {
        _tx.a.~aa();
    }
}

seastar::future<> actor_message_queue::send(seastar::shard_id t, actor_msg_service_group ssg, actor_message* msg) {
    // TODO[DAIL]: add flow control
    /*
    auto ssg_id = internal::actor_msg_service_group_id(ssg);
    auto& sem = actor_msg_service_groups[ssg_id].clients[t];
    return get_units(sem, 1).then([this, msg = std::move(msg)] (semaphore_units<> u) mutable {
        _tx.a.pending_fifo.push_back(msg);
        u.release();
        if (_tx.a.pending_fifo.size() >= batch_size) {
            move_pending();
        }
    });
    */
    _tx.a.pending_fifo.push_back(msg);
    if (_tx.a.pending_fifo.size() >= batch_size) {
        move_pending();
    }
    return seastar::make_ready_future();
}

seastar::future<> actor_message_queue::send(seastar::shard_id t, actor_message* msg) {
    _tx.a.pending_fifo.push_back(msg);
    if (_tx.a.pending_fifo.size() >= batch_size) {
        move_pending();
    }
    return seastar::make_ready_future();
    // TODO[DAIL]: rollback for flow control
    /*
    auto ssg_id = internal::actor_msg_service_group_id(default_actor_msg_service_group());
    actor_print("engine {} send msg with ssg_id {} msg.shard_id {}", local_shard_id(), ssg_id, msg->hdr.shard_id);
    auto& sem = actor_msg_service_groups[ssg_id].clients[t];
    return get_units(sem, 1).then([this, msg = std::move(msg)] (semaphore_units<> u) mutable {
        _tx.a.pending_fifo.push_back(msg);
        u.release();
        if (_tx.a.pending_fifo.size() >= batch_size) {
            move_pending();
        }
    });
    */
}

seastar::future<> actor_message_queue::send_urgent(seastar::shard_id t, actor_message* msg) {
    // Put message in the front of pending_fifo
    _tx.a.pending_fifo.push_front(msg);
    if (_tx.a.pending_fifo.size() >= batch_size) {
        move_pending();
    }
    return seastar::make_ready_future();
}

void actor_message_queue::start(unsigned cpuid) {
    _tx.init();
    namespace sm = seastar::metrics;
    char instance[10];
    std::snprintf(instance, sizeof(instance), "%u-%u", local_shard_id(), cpuid);
    _metrics.add_group("actor_msg", {
        // queue_length     value:GAUGE:0:U
        // Absolute value of num packets in last tx batch.
        sm::make_queue_length("send_batch_queue_length", _last_snt_batch, sm::description("Current send batch queue length"), {sm::shard_label(instance)})(sm::metric_disabled),
        sm::make_queue_length("receive_batch_queue_length", _last_rcv_batch, sm::description("Current receive batch queue length"), {sm::shard_label(instance)})(sm::metric_disabled),
        sm::make_queue_length("complete_batch_queue_length", _last_cmpl_batch, sm::description("Current complete batch queue length"), {sm::shard_label(instance)})(sm::metric_disabled),
        sm::make_queue_length("send_queue_length", _current_queue_length, sm::description("Current send queue length"), {sm::shard_label(instance)})(sm::metric_disabled),
        // total_operations value:DERIVE:0:U
        sm::make_derive("total_received_messages", _received, sm::description("Total number of received messages"), {sm::shard_label(instance)})(sm::metric_disabled),
        // total_operations value:DERIVE:0:U
        sm::make_derive("total_sent_messages", _sent, sm::description("Total number of sent messages"), {sm::shard_label(instance)})(sm::metric_disabled),
        // total_operations value:DERIVE:0:U
        sm::make_derive("total_completed_messages", _compl, sm::description("Total number of messages completed"), {sm::shard_label(instance)})(sm::metric_disabled)
    });
}

// TODO[DAIL] @xmqin why removed template here?
size_t actor_message_queue::process_incoming() {
    // FIXME[DAIL]: @xmqin do we really need prefetch data here?
    auto& q = _pending;
    actor_message* messages[queue_length + prefetch_cnt];
    actor_message* tm;
    if (!q.pop(tm))
        return 0;
    // start prefetching first item before popping the rest to overlap memory
    // access with potential cache miss the second pop may cause
    seastar::prefetch<2>(tm);
    auto nr = q.pop(messages);
    std::fill(std::begin(messages) + nr, std::begin(messages) + nr + prefetch_cnt, nr ? messages[nr - 1] : tm);
    unsigned i = 0;
    do {
        seastar::prefetch_n<2>(std::begin(messages) + i, std::begin(messages) + i + prefetch_cnt);
        actor_engine().receive(tm);
        tm = messages[i++];
    } while (i <= nr);

    nr = nr + 1;
    _received += nr;
    _last_rcv_batch = nr;
    return nr;
}

void actor_message_queue::stop() {
    _metrics.clear();
}

void actor_message_queue::move_pending() {
    auto begin = _tx.a.pending_fifo.cbegin();
    auto end = _tx.a.pending_fifo.cend();
    end = _pending.push(begin, end);
    if (begin == end) {
        return;
    }
    auto nr = end - begin;
    _pending.maybe_wakeup();
    _tx.a.pending_fifo.erase(begin, end);
    _current_queue_length += nr;
    _last_snt_batch = nr;
    _sent += nr;
}

void actor_message_queue::flush_request_batch() {
    if (!_tx.a.pending_fifo.empty()) {
        move_pending();
    }
}

bool actor_message_queue::pure_poll_rx() const {
    // can't use read_available(), not available on older boost
    // empty() is not const, so need const_cast.
    return !const_cast<lf_queue&>(_pending).empty();
}

void local_channel::qs_deleter::operator()(actor_message_queue** qs) const {
    for (unsigned i = 0; i < count; i++) {
        for (unsigned j = 0; j < count; j++) {
            qs[i][j].~actor_message_queue();
        }
        ::operator delete[](qs[i]);
    }
    delete[](qs);
}

local_channel::qs local_channel::_qs;
unsigned local_channel::count;

void local_channel::create_qs(const std::vector<root_actor_group*>& root_actor_groups) {
    auto nr_cpu = static_cast<unsigned>(root_actor_groups.size());
    local_channel::count = nr_cpu;
    local_channel::_qs = decltype(local_channel::_qs){new actor_message_queue* [nr_cpu], qs_deleter{nr_cpu}};
    for (unsigned i = 0; i < nr_cpu; i++) {
        local_channel::_qs[i] = reinterpret_cast<actor_message_queue*>(
            operator new[] (sizeof(actor_message_queue) * nr_cpu));
        for (unsigned j = 0; j < nr_cpu; ++j) {
            new (&local_channel::_qs[i][j]) actor_message_queue(root_actor_groups[j], root_actor_groups[i]);
        }
    }
}

bool local_channel::poll_queues() {
    size_t got = 0;
    for (unsigned i = 0; i < count; i++) {
        if (local_shard_id() != i) {
            auto& rxq = _qs[local_shard_id()][i];
            got += rxq.process_incoming();
            auto& txq = _qs[i][local_shard_id()];
            txq.flush_request_batch();
        }
    }
    return got != 0;
}

bool local_channel::pure_poll_queues() {
    for (unsigned i = 0; i < count; i++) {
        if (local_shard_id() != i) {
            auto& txq = _qs[i][local_shard_id()];
            txq.flush_request_batch();
            auto& rxq = _qs[local_shard_id()][i];
            if (rxq.pure_poll_rx()) {
                return true;
            }
        }
    }
    return false;
}

} // namespace hiactor
