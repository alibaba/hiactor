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

#include "actor/patient_groups.actg.h"
#include "generated/patients_ref.act.autogen.h"
#include <hiactor/core/actor-app.hh>
#include <seastar/core/print.hh>
#include <seastar/core/sleep.hh>

using namespace patient;
using namespace patient::outpatient;
using namespace patient::emergency;
using namespace std::chrono_literals;

seastar::future<> simulate() {
    std::vector<outpatient_patient_ref> outpatient_patient_refs;
    std::vector<emergency_patient_ref> emergency_patient_refs;
    
    hiactor::scope_builder clinic;
    clinic.enter_sub_scope(hiactor::scope<patient_group>(1)).enter_sub_scope(hiactor::scope<outpatient_group>(1));
    outpatient_patient_refs.reserve(6);
    for (unsigned i = 0; i < 6; i++) {
        clinic.set_shard(i % hiactor::local_shard_count());
        outpatient_patient_refs.push_back(clinic.build_ref<outpatient_patient_ref>(i));
    }
    
    clinic.back_to_parent_scope().enter_sub_scope(hiactor::scope<emergency_group>(2));
    emergency_patient_refs.reserve(6);
    for (unsigned i = 0; i < 6; i++) {
        clinic.set_shard(i % hiactor::local_shard_count());
        emergency_patient_refs.push_back(clinic.build_ref<emergency_patient_ref>(i));
    }

    // trigger the actor check_info method by the default order.
    // the actual execution order is determined by our customized scheduling policies.
    seastar::future<> outpatient_loop = seastar::parallel_for_each(boost::irange(0u, 6u), [&outpatient_patient_refs](int i) {
        return outpatient_patient_refs[i].treat(hiactor::Integer(i)).then_wrapped([i] (seastar::future<hiactor::Integer> fut) {
            auto& patient_name = outpatient_dp::get().find(i).name;
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
                fmt::print("Fail: {}", ex_content);
            } else {
                fmt::print("Successfully get registration id: {}\n", fut.get0().val);
            }
        });
    });

    seastar::future<> emergency_loop = seastar::parallel_for_each(boost::irange(0u, 6u), [&emergency_patient_refs](int j) {
        return emergency_patient_refs[j].treat(hiactor::Integer(j)).then_wrapped([j] (seastar::future<hiactor::Integer> fut) {
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
                fmt::print("Fail: {}", ex_content);
            } else {
                fmt::print("Successfully get emergency level: {}\n", fut.get0().val);
            }
        });
    });

    return seastar::when_all_succeed(std::move(outpatient_loop), std::move(emergency_loop));
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
