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

#include "actor/bank_account.act.h"
#include <seastar/core/print.hh>

seastar::future<hiactor::Integer> bank_account::withdraw(hiactor::Integer&& amount) {
    auto request = amount.val;
    if (request <= 0) {
        return seastar::make_exception_future<hiactor::Integer>(std::runtime_error(
                "The withdrawal amount must be greater than 0."));
    } else if (request > balance) {
        return seastar::make_exception_future<hiactor::Integer>(std::runtime_error(
                seastar::format("Account balance is not enough, request: {}, remaining: {}.", request, balance)));
    } else {
        balance -= request;
        return seastar::make_ready_future<hiactor::Integer>(balance);
    }
}

seastar::future<hiactor::Integer> bank_account::deposit(hiactor::Integer&& amount) {
    auto request = amount.val;
    if (request <= 0) {
        return seastar::make_exception_future<hiactor::Integer>(std::runtime_error(
                "The deposit amount must be greater than 0."));
    } else {
        balance += request;
        return seastar::make_ready_future<hiactor::Integer>(balance);
    }
}

seastar::future<hiactor::Integer> bank_account::check() {
    return seastar::make_ready_future<hiactor::Integer>(balance);
}
