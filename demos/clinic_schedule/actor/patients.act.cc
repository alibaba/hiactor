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

#include "actor/patients.act.h"
#include "actor/patient_dp.h"
#include <seastar/core/print.hh>

namespace patient {

seastar::future<hiactor::Integer> generic_patient::treat(hiactor::Integer&& num) {
    if (num.val > _num) {
        return seastar::make_exception_future<hiactor::Integer>(std::make_exception_ptr(std::runtime_error(
            seastar::format("There are not so many patients, request: {}, remaining: {}\n", num.val, _num))));
    } else {
        _num -= num.val;
    }
    return seastar::make_ready_future<hiactor::Integer>(_num);
}

namespace outpatient {

outpatient_patient::outpatient_patient(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
    : generic_patient(exec_ctx, addr) {
    // init the outpatient patient with its metadata
    add_meta("outpatient", outpatient_dp::get().find(_patient_id));
}

seastar::future<hiactor::Integer> outpatient_patient::check_info() {
    auto* info = get_meta_ptr<outpatient_info>("outpatient");
    fmt::print("[Shard: {}][outpatient: id({}), name({}), registration_id({}), num({})]\n",
               hiactor::local_shard_id(), _patient_id, info->name, info->registration_id, _num);
    return seastar::make_ready_future<hiactor::Integer>(info->registration_id);
}

} // namespace outpatient

namespace emergency {

emergency_patient::emergency_patient(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
    : generic_patient(exec_ctx, addr) {
    // init the emergency patient with its metadata
    add_meta("emergency", emergency_dp::get().find(_patient_id));
}

seastar::future<hiactor::Integer> emergency_patient::check_info() {
    auto* info = get_meta_ptr<emergency_info>("emergency");
    fmt::print("[Shard: {}][emergency: id({}), name({}), emergency_level({}), num({})]\n",
               hiactor::local_shard_id(), _patient_id, info->name, info->emergency_level, _num);
    return seastar::make_ready_future<hiactor::Integer>(info->emergency_level);
}

} // namespace emergency

} // namespace patient
