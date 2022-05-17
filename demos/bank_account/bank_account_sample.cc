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
#include "generated/bank_account_ref.act.autogen.h"
#include <hiactor/core/actor-app.hh>
#include <seastar/core/print.hh>

seastar::future<> simulate() {
    // Create a bank account with id `10` at actor group `1`, use an actor reference to trigger its actor methods.
    uint32_t ba_id = 10;
    hiactor::scope_builder ba_builder;
    ba_builder
        .set_shard(ba_id % hiactor::global_shard_count())
        .enter_sub_scope(hiactor::scope<hiactor::actor_group>(1));
    return seastar::do_with(ba_builder.build_ref<bank_account_ref>(ba_id), [] (bank_account_ref& ba_ref) {
        fmt::print("Deposit 5 into account...\n");
        return ba_ref.deposit(hiactor::Integer(5)).then([] (hiactor::Integer balance) {
            fmt::print("Successful deposit, current balance: {}\n", balance.val);
        }).then([&ba_ref] {
            fmt::print("Withdraw 8 from account...\n");
            return ba_ref.withdraw(hiactor::Integer(8)).then_wrapped([] (seastar::future<hiactor::Integer> fut) {
                // this call with fail with an exception as remaining balance is not enough for 8.
                try {
                    std::rethrow_exception(fut.get_exception());
                } catch (const std::exception& e) {
                    fmt::print("Failed withdrawal: {}\n", e.what());
                }
            });
        }).then([&ba_ref] {
            return ba_ref.check().then([] (hiactor::Integer balance) {
                fmt::print("Account checked, current balance: {}\n", balance.val);
            });
        }).then([&ba_ref] {
            fmt::print("Withdraw 3 from account again...\n");
            return ba_ref.withdraw(hiactor::Integer(3)).then_wrapped([] (seastar::future<hiactor::Integer> fut) {
                // this call with succeed as remaining balance is enough for 3.
                auto balance = fut.get0().val;
                fmt::print("Successful withdrawal, current balance: {}\n", balance);
            });
        });
    });
}

int main(int ac, char** av) {
    hiactor::actor_app app;
    app.run(ac, av, [] {
        return simulate().then([] {
            hiactor::actor_engine().exit();
            fmt::print("Exit actor system.\n");
        });
    });
}
