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

#include "actor/item_groups.actg.h"
#include "generated/actor/items_ref.act.autogen.h"
#include <hiactor/core/actor-app.hh>
#include <seastar/core/print.hh>
#include <seastar/core/sleep.hh>

using namespace item;
using namespace item::food;
using namespace item::medicine;
using namespace std::chrono_literals;

std::vector<food_item_ref> food_item_refs;
std::vector<medicine_item_ref> medicine_item_refs;

seastar::future<> simulate() {
    // create a customized schedulable food actor group with id "1" to manage food actors, the food
    // actor group will be managed by the superior item actor group with id "1" hierarchically.
    hiactor::scope_builder food_builder(0, hiactor::scope<item_group>(1), hiactor::scope<food_group>(1));
    food_item_refs.reserve(6);
    for (unsigned i = 0; i < 6; i++) {
        food_item_refs.push_back(food_builder.build_ref<food_item_ref>(i));
    }
    // create a customized schedulable medicine actor group with id "2" to manage medicine actors, the
    // medicine actor group will be managed by the superior item actor group with id "1" hierarchically.
    hiactor::scope_builder medicine_builder(0, hiactor::scope<item_group>(1), hiactor::scope<medicine_group>(2));
    medicine_item_refs.reserve(6);
    for (unsigned i = 0; i < 6; i++) {
        medicine_item_refs.push_back(medicine_builder.build_ref<medicine_item_ref>(i));
    }

    // trigger the actor print_info method by the default order.
    // the actual execution order is determined by our customized scheduling policies.
    for (auto& ref : food_item_refs) {
        ref.print_info();
    }
    for (auto& ref : medicine_item_refs) {
        ref.print_info();
    }
    return seastar::sleep(1s);
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