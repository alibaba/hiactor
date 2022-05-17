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
#include "actor/item_store.h"

/// This file define some item actors which runs the behaviours in your actor app.
///
/// Customized actors should be defined in c++ headers with the suffix ".act.h",
/// you can also separate your customized actors into different ".act.h" headers
/// according to your needs.
///
/// Each customized actor should be derived from the temple actor types in "hiactor/core/actor-template.hh"
/// Each customized actor should use an annotation "ANNOTATION(actor:impl)" to identify itself.
/// Each actor method in customized actors should be annotated by "ANNOTATION(actor:method)".
/// Each actor method should not contain more than one argument.
/// Each actor method's argument type should be a rvalue-reference to support moving.
///
/// All the actor methods are executed asynchronously, if you don't want to wait an actor method to be finished,
/// the return type of this actor method should be "void", otherwise, you should return a "seastar::future<ResultType>"
/// of your waiting result.
///
/// For each customized actor, the hiactor codegen tools will generate an actor reference class corresponding
/// to it, e.g. (food_item -> food_item_ref), each actor reference has the same methods as those annotated in
/// its pair actor.
///
/// Use "hiactor::scope_builder" to build an actor reference with hierarchical scopes, the built actor
/// ref indicates the \shard and \scope information where the corresponding actor is located. Calling
/// the actor method from an actor reference will trigger the true actor behaviours to be run.

namespace item {

class ANNOTATION(actor:impl) generic_item : public hiactor::stateful_actor {
protected:
    unsigned _item_id;
    int _stock = 10; // init stock of 10.
public:
    generic_item(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::stateful_actor(exec_ctx, addr) {
        _item_id = actor_id();
    }
    ~generic_item() override = default;

    /// Define your actor behaviour methods here.
    /// The customized actor behaviours should use an annotation to identify it.
    /// Actor behaviours can be virtual for further inheritance.

    /// Buy this item with quantity "num".
    /// Return the remaining stock of this item, if current stock cannot meet the purchase
    /// demand of quantity @num, an exception future will be returned.
    virtual seastar::future<hiactor::Integer> ANNOTATION(actor:method) buy(hiactor::Integer&& num);

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

namespace food {

class ANNOTATION(actor:impl) food_item : public generic_item {
public:
    food_item(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr);
    ~food_item() override = default;

    /// Print the information of this food item.
    void ANNOTATION(actor:method) print_info();

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

} // namespace food

namespace medicine {

class ANNOTATION(actor:impl) medicine_item : public generic_item {
public:
    medicine_item(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr);
    ~medicine_item() override = default;

    /// Print the information of this medicine item.
    void ANNOTATION(actor:method) print_info();

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

} // namespace medicine

} // namespace item
