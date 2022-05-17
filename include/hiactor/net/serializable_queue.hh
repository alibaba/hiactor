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

#include <seastar/core/temporary_buffer.hh>

namespace hiactor {

using byte_tmpbuf_t = seastar::temporary_buffer<char>;
const int ARRAY_BUFFER_SIZE = 4;

struct tmpbuf_array {
    unsigned size = 0;
    unsigned cap = ARRAY_BUFFER_SIZE;
    byte_tmpbuf_t buffers[ARRAY_BUFFER_SIZE];
    unsigned cur_idx = 0;

    // constructors:
    tmpbuf_array() = default;
    // copy constructor of seastar::temporary_buffer is deleted
    tmpbuf_array(const tmpbuf_array&) = delete;
    tmpbuf_array(tmpbuf_array&& other) noexcept:
        size(other.size), cap(other.cap) {
        // move each element in the array
        std::move(std::begin(other.buffers), std::end(other.buffers), buffers);
    }
    ~tmpbuf_array() = default;

    bool is_full() const {
        return size == cap;
    };

    bool empty() const {
        return cur_idx == size;
    }

    void push(byte_tmpbuf_t&& buf) {
        buffers[size] = std::move(buf);
        size++;
    };

    // only call pop() when there is a buffer in the array
    byte_tmpbuf_t pop() {
        unsigned tmp_idx = cur_idx;
        cur_idx++;
        return std::move(buffers[tmp_idx]);
    }

    // only call pop() when there is a buffer in the array
    const byte_tmpbuf_t& peek() const {
        return buffers[cur_idx];
    }

    unsigned get_buffer_size() const {
        unsigned total_buffer_size = 0;
        for (unsigned i = cur_idx; i < size; i++) {
            total_buffer_size += buffers[i].size();
        }
        return total_buffer_size;
    }
};


// linked list
struct tmpbuf_linked_list {

    // linked list node
    struct tmpbuf_list_node {
        // each node stores an array of temporary buffers
        tmpbuf_array buffer_array;
        tmpbuf_list_node* next = nullptr;

        // constructors
        tmpbuf_list_node() = default;
        tmpbuf_list_node(tmpbuf_list_node&& other) noexcept:
            buffer_array(std::move(other.buffer_array)), next(other.next) {
            other.next = nullptr;
        }
        tmpbuf_list_node(const tmpbuf_list_node& other) = delete;
        ~tmpbuf_list_node() = default;
    };

    // total number of nodes in the linked list
    // not the number of buffers
    unsigned node_count = 0;

    tmpbuf_list_node* head = nullptr;
    tmpbuf_list_node* tail = nullptr;

    tmpbuf_linked_list() = default;
    tmpbuf_linked_list(const tmpbuf_linked_list&) = delete;
    tmpbuf_linked_list(tmpbuf_linked_list&& other) noexcept: head(other.head), tail(other.tail) {
        other.head = nullptr;
        other.tail = nullptr;
    }

    ~tmpbuf_linked_list() {
        // release memory when destroy
        auto cur = head;
        for (; cur != nullptr;) {
            auto next = cur->next;
            delete cur;
            cur = next;
        }
    }

    // push a buffer to the tail
    void push(byte_tmpbuf_t&& buf) {
        // if the list is empty or the current node is full, create a new node
        if (node_count == 0 || tail->buffer_array.is_full()) {
            // create a new node
            auto* node = new tmpbuf_list_node();
            node->buffer_array.push(std::move(buf));

            if (node_count == 0) {
                head = node;
                tail = node;
            } else {
                tail->next = node;
                tail = tail->next;
            }
            node_count++;
        } else {
            tail->buffer_array.push(std::move(buf));
        }
    }

    // only call pop() when there is a buffer in the list
    byte_tmpbuf_t pop() {
        while (head != nullptr) {
            if (!head->buffer_array.empty()) {
                return head->buffer_array.pop();
            }
            // all the buffers in the head node is popped
            // head points to its next
            node_count--;
            head = head->next;
        }

        return {};
    }

    // only call peek() when there is a buffer in the list.
    const byte_tmpbuf_t& peek() const {
        return head->buffer_array.peek();
    }

    unsigned get_buffer_size() const {
        unsigned total_size = 0;
        tmpbuf_list_node* tmp = head;
        while (tmp != nullptr) {
            total_size += tmp->buffer_array.get_buffer_size();
            tmp = tmp->next;
        }
        return total_size;
    }
};


class serializable_queue {
    // fixed size array of buffers on the stack
    tmpbuf_array _array;
    // linked list when fixed size array is full, add to linked list
    tmpbuf_linked_list _list;
    // total number of buffers
    unsigned _size = 0;

public:
    // if the fixed size array is not full, push to the array
    // otherwise push to the linked list
    void push(byte_tmpbuf_t&& buf) {
        _size++;
        if (_array.is_full()) {
            _list.push(std::move(buf));
        } else {
            _array.push(std::move(buf));
        }
    };

    // only call peek when there is a buffer
    // peek does not transfer the ownership of the buffer
    const byte_tmpbuf_t& peek() const {
        if (_array.empty()) {
            return _list.peek();
        }
        return _array.peek();
    }

    // only call pop() when there is a buffer
    // the ownership of the buffer is transferred
    byte_tmpbuf_t pop() {
        _size--;
        // if the array is empty, then pop from the list
        if (_array.empty()) {
            return _list.pop();
        }
        return _array.pop();
    }

    bool empty() const {
        return _size == 0;
    }

    unsigned size() const {
        return _size;
    }

    unsigned get_buffer_size() const {
        return _array.get_buffer_size() + _list.get_buffer_size();
    }
};

} // namespace hiactor
