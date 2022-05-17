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

#include <hiactor/core/root_actor_group.hh>
#include <hiactor/net/network_channel.hh>

#include <seastar/core/prefetch.hh>

namespace hiactor {

void network_message_queue::lf_queue::maybe_wakeup() {
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

network_message_queue::network_message_queue(root_actor_group* from, root_actor_group* to)
    : _pending(to), _completed(from) {}

network_message_queue::~network_message_queue() {
    if (_pending.remote != _completed.remote) {
        _tx.a.~aa();
    }
}

seastar::future<> network_message_queue::send(unsigned t, message_item mi, normal_message_tag) {
    _tx.a.pending_fifo.push_back(mi);
    if (_tx.a.pending_fifo.size() >= batch_size) {
        move_pending();
    }
    return seastar::make_ready_future();
}

seastar::future<> network_message_queue::send(unsigned t, message_item mi, urgent_message_tag) {
    // Put message in the front of pending_fifo
    _tx.a.pending_fifo.push_front(mi);
    if (_tx.a.pending_fifo.size() >= batch_size) {
        move_pending();
    }
    return seastar::make_ready_future();
}

void network_message_queue::start(unsigned cpuid) {
    _tx.init();
    namespace sm = seastar::metrics;
    char instance[10];
    std::snprintf(instance, sizeof(instance), "%u-%u", local_shard_id(), cpuid);
    _metrics.add_group("network_message", {
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

size_t network_message_queue::process_incoming() {
    // FIXME[DAIL]: @xmqin do we really need prefetch data here?
    auto& q = _pending;
    message_item messages[queue_length + prefetch_cnt];
    message_item tm{};
    if (!q.pop(tm))
        return 0;
    // start prefetching first item before popping the rest to overlap memory
    // access with potential cache miss the second pop may cause
    seastar::prefetch<2>(&tm);
    auto nr = q.pop(messages);
    std::fill(std::begin(messages) + nr, std::begin(messages) + nr + prefetch_cnt, nr ? messages[nr - 1] : tm);
    unsigned i = 0;
    message_item* mi_ptr = &tm;
    do {
        std::for_each(std::begin(messages) + i,
                      std::begin(messages) + i + prefetch_cnt,
                      [](auto v) { seastar::prefetch<2>(&v); });
        network_channel::push_to_buffer(mi_ptr->mach_id, mi_ptr->msg);
        mi_ptr = &messages[i++];
    } while (i <= nr);

    nr = nr + 1;
    _received += nr;
    _last_rcv_batch = nr;
    return nr;
}

void network_message_queue::move_pending() {
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

void network_message_queue::stop() {
    _metrics.clear();
}

void network_message_queue::flush_request_batch() {
    if (!_tx.a.pending_fifo.empty()) {
        move_pending();
    }
}

bool network_message_queue::pure_poll_rx() const {
    // can't use read_available(), not available on older boost
    // empty() is not const, so need const_cast.
    return !const_cast<lf_queue&>(_pending).empty();
}

void network_channel::qs_deleter::operator()(network_message_queue** qs) const {
    for (unsigned i = 0; i < count; i++) {
        for (unsigned j = 0; j < count; j++) {
            qs[i][j].~network_message_queue();
        }
        ::operator delete[](qs[i]);
    }
    delete[](qs);
}

network_channel::qs network_channel::_qs;
unsigned network_channel::count;
network_channel::routing_table network_channel::_routing_tbl;
entry<connection_manager>* network_channel::_cms{nullptr};

void network_channel::create_qs(const std::vector<root_actor_group*>& root_actor_groups) {
    auto nr_cpu = static_cast<unsigned>(root_actor_groups.size());
    network_channel::count = nr_cpu;
    network_channel::_qs = decltype(network_channel::_qs){new network_message_queue* [nr_cpu], qs_deleter{nr_cpu}};
    for (unsigned i = 0; i < nr_cpu; i++) {
        network_channel::_qs[i] = reinterpret_cast<network_message_queue*>(
            operator new[] (sizeof(network_message_queue) * nr_cpu));
        for (unsigned j = 0; j < nr_cpu; ++j) {
            new (&network_channel::_qs[i][j]) network_message_queue(root_actor_groups[j], root_actor_groups[i]);
        }
    }
}

bool network_channel::poll_queues() {
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

bool network_channel::pure_poll_queues() {
    for (unsigned i = 0; i < count; i++) {
        auto& txq = _qs[i][local_shard_id()];
        txq.flush_request_batch();
        auto& rxq = _qs[local_shard_id()][i];
        if (rxq.pure_poll_rx()) {
            return true;
        }
    }
    return false;
}

} // namespace hiactor
