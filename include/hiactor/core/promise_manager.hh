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

#include <seastar/core/future.hh>

namespace hiactor {

#if SEASTAR_API_LEVEL < 6
template <class... T>
#else
template <class T = void>
#endif
struct pr_manager {
    struct node {
        seastar::promise<T SEASTAR_ELLIPSIS> pr;
        uint32_t next_avl;
        bool active;

        node() : pr(), next_avl(0), active(true) {}
        node(const node&) = delete;
        node(node&& other) noexcept : pr(std::move(other.pr)), next_avl(other.next_avl), active(other.active) {}
        ~node() = default;

        void occupy() {
            pr = seastar::promise<T SEASTAR_ELLIPSIS>{};
            active = true;
        }

        void release() {
            active = false;
        }
    };
    std::vector<node> pr_array{};
    uint32_t available;

    explicit pr_manager(const size_t init_reserve_size = 0) : available(0) {
        pr_array.reserve(init_reserve_size + 1);
        pr_array.emplace_back();
        using value_type = typename seastar::future<T SEASTAR_ELLIPSIS>::value_type;
        pr_array[0].pr.set_value(value_type{});
        pr_array[0].release();
    }
    ~pr_manager() = default;

    uint32_t acquire_pr() {
        if (available == 0) {
            pr_array.emplace_back();
            return static_cast<uint32_t>(pr_array.size() - 1);
        }
        auto pr_id = available;
        available = pr_array[pr_id].next_avl;
        pr_array[pr_id].occupy();
        return pr_id;
    }

    void remove_pr(uint32_t pr_id) {
        pr_array[pr_id].release();
        pr_array[pr_id].next_avl = available;
        available = pr_id;
    }

    size_t size() const {
        return pr_array.size();
    }

    bool is_active(uint32_t pr_id) const {
        return pr_array[pr_id].active;
    }

    seastar::future<T SEASTAR_ELLIPSIS> get_future(uint32_t pr_id) noexcept {
        return pr_array[pr_id].pr.get_future();
    }

    template <typename... A>
    void set_value(uint32_t pr_id, A&& ... a) noexcept {
        pr_array[pr_id].pr.set_value(std::forward<A>(a)...);
    }

    void set_exception(uint32_t pr_id, std::exception_ptr ex) noexcept {
        pr_array[pr_id].pr.set_exception(std::move(ex));
    }
};

} // namespace hiactor
