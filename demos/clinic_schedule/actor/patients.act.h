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

#pragma once

#include <hiactor/core/actor-template.hh>
#include <hiactor/util/data_type.hh>
#include "actor/patient_dp.h"

/// This file define some patient actors which runs the behaviours in your actor app.
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
/// to it, e.g. (outpatient_patient -> outpatient_patient_ref), each actor reference has the same methods as those annotated in
/// its pair actor.
///
/// Use "hiactor::scope_builder" to build an actor reference with hierarchical scopes, the built actor
/// ref indicates the \shard and \scope information where the corresponding actor is located. Calling
/// the actor method from an actor reference will trigger the true actor behaviours to be run.

namespace patient {

class ANNOTATION(actor:impl) generic_patient : public hiactor::stateful_actor {
protected:
    unsigned _patient_id;
    int _num = 4; // init num of 10.
public:
    generic_patient(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::stateful_actor(exec_ctx, addr) {
        _patient_id = actor_id();
    }
    ~generic_patient() override = default;

    /// Define your actor behaviour methods here.
    /// The customized actor behaviours should use an annotation to identify it.
    /// Actor behaviours can be virtual for further inheritance.

    /// Treat this patient with quantity "num".
    /// Return the remaining num of this patient, if current num cannot meet the purchase
    /// demand of quantity @num, an exception future will be returned.
    virtual seastar::future<hiactor::Integer> ANNOTATION(actor:method) treat(hiactor::Integer&& num);

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

namespace outpatient {

class ANNOTATION(actor:impl) outpatient_patient : public generic_patient {
public:
    outpatient_patient(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr);
    ~outpatient_patient() override = default;

    /// Print the information of this outpatient patient.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) check_info();

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

} // namespace outpatient

namespace emergency {

class ANNOTATION(actor:impl) emergency_patient : public generic_patient {
public:
    emergency_patient(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr);
    ~emergency_patient() override = default;

    /// Print the information of this emergency patient.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) check_info();

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

} // namespace emergency

} // namespace patient
