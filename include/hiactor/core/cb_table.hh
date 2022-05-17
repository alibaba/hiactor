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

#include <hiactor/core/column_batch.hh>
#include <hiactor/net/serializable_queue.hh>

namespace hiactor {
namespace cb {

/**
 * Table is a wrapper to orchestrate column batches.
 * 1. Table is not shareable
 * 2. Column batch is the basic unit to share.
 */
template <typename... Columns>
struct table {
    std::tuple<Columns...> cols;
    explicit table(Columns&& ... cols) : cols(std::forward<Columns>(cols)...) {}
    ~table() = default;

    table(const table&) = delete;
    table(table&& x) noexcept: cols(std::move(x.cols)) {}

    void dump_to(serializable_queue& qu) {
        foreach_dump_to(qu);
    }

private:
    template <size_t I = 0>
    std::enable_if_t<I == sizeof...(Columns), void> foreach_dump_to(serializable_queue& qu) {}

    template <size_t I = 0>
    std::enable_if_t<I < sizeof...(Columns), void> foreach_dump_to(serializable_queue& qu) {
        std::get<I>(cols).dump_to(qu);
        foreach_dump_to<I + 1>(qu);
    }
};

template <typename First, typename... Rest>
struct all_is_rvalue_reference {
    static const bool value = std::is_rvalue_reference<First&&>::value && all_is_rvalue_reference<Rest...>::value;
};

template <typename First>
struct all_is_rvalue_reference<First> {
    static const bool value = std::is_rvalue_reference<First&&>::value;
};

template <typename... Columns, typename = std::enable_if_t<all_is_rvalue_reference<Columns...>::value>>
inline
table<Columns...> make_table(Columns&& ... cols) {
    return table<Columns...>{std::forward<Columns>(cols)...};
}

} // namespace cb
} // namespace hiactor
