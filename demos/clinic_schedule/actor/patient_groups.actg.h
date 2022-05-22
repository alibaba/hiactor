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

#include "actor/patient_dp.h"
#include <hiactor/core/actor-template.hh>

/// This file define some patient actor groups with scheduling ability, which
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

namespace patient {

namespace outpatient {

/// A outpatient actor group will manage all the outpatient patient actors.
class ANNOTATION(actor:group) outpatient_group : public hiactor::schedulable_actor_group {
public:
    outpatient_group(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::schedulable_actor_group(exec_ctx, addr) {
        /// Add metadata of current actor group
        add_meta("group_name", std::string("outpatient_group"));
    }
    /// Comparator Func of two scheduling actors，needs to be overwritten in derived actor groups.
    /// \return `true`  : the priority of actor task a is lower than actor task b.
    /// \return `false` : the priority of actor task a is larger than or equal to actor task b.
    bool compare(const actor_base* a, const actor_base* b) const override {
        /// In outpatient actor group, patients with earlier registeration id will be priorly scheduled.
        return a->get_meta_ptr<outpatient_info>("outpatient")->registration_id
            > b->get_meta_ptr<outpatient_info>("outpatient")->registration_id;
    }
};

} // namespace outpatient

namespace emergency {

/// A emergency actor group will manage all the emergency patient actors.
class ANNOTATION(actor:group) emergency_group : public hiactor::schedulable_actor_group {
public:
    emergency_group(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::schedulable_actor_group(exec_ctx, addr) {
        /// add metadata of current actor group
        add_meta("group_name", std::string("emergency_group"));
    }
    bool compare(const actor_base* a, const actor_base* b) const override {
        /// In emergency actor group, patients with higher emergency level will be priorly scheduled.
        return a->get_meta_ptr<emergency_info>("emergency")->emergency_level
            < b->get_meta_ptr<emergency_info>("emergency")->emergency_level;
    }
};

} // namespace emergency

/// An patient actor group will manage outpatient actor group and emergency actor group hierarchically.
class ANNOTATION(actor:group) patient_group : public hiactor::schedulable_actor_group {
public:
    patient_group(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::schedulable_actor_group(exec_ctx, addr) {}
    bool compare(const actor_base* a, const actor_base* b) const override {
        /// We simply assume emergency group has a higher priority, so only when "a" is a
        /// outpatient group and "b" is a emergency group, we need to return `true`.
        return a->get_meta<std::string>("group_name") != "emergency_group" &&
            b->get_meta<std::string>("group_name") == "emergency_group";
    }
};

} // namespace patient
