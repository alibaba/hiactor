/** Copyright 2022 Alibaba Group Holding Limited. All Rights Reserved.
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

#include "actor_harness.hh"
#include <hiactor/core/actor-app.hh>
#include <hiactor/testing/test_case.hh>
#include <seastar/core/print.hh>

HIACTOR_TEST_CASE(actor) {
    uint32_t cid = 10;
    hiactor::scope_builder count_builder;
    count_builder.set_shard(cid % hiactor::global_shard_count()).enter_sub_scope(hiactor::scope<hiactor::actor_group>(1));
    return seastar::do_with(count_builder.build_ref<count_ref>(cid), [&](count_ref &ref) {
        return ref.increase(hiactor::Integer(45)).then([&](hiactor::Integer amount) {
            BOOST_CHECK_EQUAL(amount.val, 45);
        }).then([&ref] {
            return ref.increase(hiactor::Integer(1)).then([](hiactor::Integer amount) {
                BOOST_CHECK_EQUAL(amount.val, 46);
            });
        }).then([&ref] {
            return ref.increase(hiactor::Integer(-10)).then_wrapped([](seastar::future<hiactor::Integer> fut) {
                try {
                    BOOST_CHECK_THROW(std::rethrow_exception(fut.get_exception()), std::logic_error);
                }
                catch (...) {}
            });
        }).then([&ref] {
            return ref.check().then([&](hiactor::Integer amount) {
                BOOST_CHECK_EQUAL(amount.val, 46);
            });
        });
    });
}
