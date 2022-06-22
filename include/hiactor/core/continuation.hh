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

#include <hiactor/core/exception.hh>

#include <seastar/core/future.hh>

namespace hiactor {

/// actor continuation task
#if SEASTAR_API_LEVEL < 6
template <typename Func, typename... T>
#else
template <typename Func, typename T = void>
#endif
struct continuation final : public seastar::continuation_base<T SEASTAR_ELLIPSIS> {
    explicit continuation(Func&& func) noexcept
        : seastar::continuation_base<T SEASTAR_ELLIPSIS>(), _func(std::move(func)) {}

    void run_and_dispose() noexcept override {
        _func(this->_state);
        delete this;
    }

    void cancel() override {
        if (!this->_state.failed()) {
            this->_state = {};
            this->_state.set_exception(std::make_exception_ptr(
                task_canceled_exception{"continuation task canceled from external!"}));
        }
    }

    Func _func;
};

} // namespace hiactor
