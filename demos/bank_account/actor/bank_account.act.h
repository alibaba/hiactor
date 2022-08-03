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

#include <hiactor/core/actor-template.hh>
#include <hiactor/util/data_type.hh>

class ANNOTATION(actor:impl) bank_account : public hiactor::actor {
    int balance = 0;
public:
    bank_account(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr) : hiactor::actor(exec_ctx, addr, false) {}
    ~bank_account() override = default;

    /// Withdraw from bank account by `amount`, return the remaining balance after withdrawing,
    /// if current balance is not enough for `amount`, an exception future will be returned.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) withdraw(hiactor::Integer&& amount);

    /// Deposit `amount` into bank account, return the remaining balance after depositing.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) deposit(hiactor::Integer&& amount);

    /// Check account info and return the balance.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) check();

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};
