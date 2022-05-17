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

#include "actor/item_store.h"
#include <hiactor/core/actor-template.hh>

/// This file define some item actor groups with scheduling ability, which
/// are derived from "hiactor::schedulable_actor_group"
///
/// Customized actor groups should be defined in c++ headers with the suffix ".actg.h",
/// you can also separate your customized actor groups into different ".actg.h" headers
/// according to your needs.
///
/// Each customized actor group should use an annotation "ANNOTATION(actor:group)" to
/// identify itself.
///
/// If your don't care the scheduling when running your actor apps, using the
/// default "hiactor::actor_group" is enough.

namespace item {

namespace food {

/// A food actor group will manage all the food item actors.
class ANNOTATION(actor:group) food_group : public hiactor::schedulable_actor_group {
public:
    food_group(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::schedulable_actor_group(exec_ctx, addr) {
        /// Add metadata of current actor group
        add_meta("group_name", std::string("food_group"));
    }
    /// Comparator Func of two scheduling actors，needs to be overwritten in derived actor groups.
    /// \return `true`  : the priority of actor task a is lower than actor task b.
    /// \return `false` : the priority of actor task a is larger than or equal to actor task b.
    bool compare(const actor_base* a, const actor_base* b) const override {
        /// In food actor group, items with earlier production dates will be priorly scheduled.
        return a->get_meta_ptr<food_info>("food")->production_date
            > b->get_meta_ptr<food_info>("food")->production_date;
    }
};

} // namespace food

namespace medicine {

/// A medicine actor group will manage all the medicine item actors.
class ANNOTATION(actor:group) medicine_group : public hiactor::schedulable_actor_group {
public:
    medicine_group(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::schedulable_actor_group(exec_ctx, addr) {
        /// add metadata of current actor group
        add_meta("group_name", std::string("medicine_group"));
    }
    bool compare(const actor_base* a, const actor_base* b) const override {
        /// In medicine actor group, items with higher urgency level will be priorly scheduled.
        return a->get_meta_ptr<medicine_info>("medicine")->urgency_level
            < b->get_meta_ptr<medicine_info>("medicine")->urgency_level;
    }
};

} // namespace medicine

/// An item actor group will manage food actor group and medicine actor group hierarchically.
class ANNOTATION(actor:group) item_group : public hiactor::schedulable_actor_group {
public:
    item_group(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::schedulable_actor_group(exec_ctx, addr) {}
    bool compare(const actor_base* a, const actor_base* b) const override {
        /// We simply assume medicine group has a higher priority, so only when "a" is a
        /// food group and "b" is a medicine group, we need to return `true`.
        return a->get_meta<std::string>("group_name") != "medicine_group" &&
            b->get_meta<std::string>("group_name") == "medicine_group";
    }
};

} // namespace item
