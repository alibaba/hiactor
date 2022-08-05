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

#include "actor/item_store.h"
#include "generated/actor/items_ref.act.autogen.h"
#include <hiactor/core/actor-app.hh>
#include <seastar/core/print.hh>
#include <seastar/core/sleep.hh>

using namespace item;
using namespace item::food;
using namespace std::chrono_literals;

std::vector<food_item_ref> food_item_refs;

seastar::future<> simulate() {
    // create a default actor group with id "1" to manage food actors.
    hiactor::scope_builder food_builder;
    food_builder.enter_sub_scope(hiactor::scope<hiactor::actor_group>(1));
    food_item_refs.reserve(6);
    for (unsigned i = 0; i < 6; i++) {
        // distribute items to different shards.
        food_builder.set_shard(i % hiactor::local_shard_count());
        food_item_refs.push_back(food_builder.build_ref<food_item_ref>(i));
    }
    return seastar::parallel_for_each(boost::irange<int>(0u, 6u),[] (int i) {
        return food_item_refs[i].buy(hiactor::Integer(i + 6)
        ).then_wrapped([i] (seastar::future<hiactor::Integer> fut) {
            auto& item_name = food_store::get().find(i).name;
            if (fut.failed()) {
                // process actor method exception
                std::string ex_content;
                try {
                    std::rethrow_exception(fut.get_exception());
                } catch (const std::exception& e) {
                    ex_content = e.what();
                } catch (...) {
                    ex_content = "unknown exception";
                }
                fmt::print("Failed purchase of {}: {}\n", item_name, ex_content);
            } else {
                fmt::print("Successful purchase of {}, remaining stock: {}\n", item_name, fut.get0().val);
            }
        });
    }).then([] {
        // display info
        for (auto& ref: food_item_refs) {
            ref.print_info();
        }
        return seastar::sleep(1s);
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