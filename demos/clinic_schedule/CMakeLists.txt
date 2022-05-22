# Copyright 2022 Alibaba Group Holding Limited. All Rights Reserved.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

hiactor_codegen (clinic-schedule-sample-autogen clinic_schedule_sample_gen_files
  ACTOR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  HIACTOR_INCLUDE ${CMAKE_SOURCE_DIR}/include
  SYSTEM_INCLUDE ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/include
  USER_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})


hiactor_add_demo (clinic_schedule_sample
  SOURCES
    clinic_schedule_sample.cc
    actor/patient_groups.actg.h
    actor/patient_dp.h
    actor/patients.act.h
    actor/patients.act.cc
    ${clinic_schedule_sample_gen_files})

add_dependencies (clinic_schedule_sample_demo clinic-schedule-sample-autogen)
