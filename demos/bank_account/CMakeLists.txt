# Copyright 2021 Alibaba Group Holding Limited. All Rights Reserved.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

hiactor_codegen (bank-account-autogen bank_account_autogen_files
  ACTOR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  HIACTOR_INCLUDE ${CMAKE_SOURCE_DIR}/include
  SYSTEM_INCLUDE ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/include
  USER_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})

hiactor_add_demo (bank_account_sample
  SOURCES
    bank_account_sample.cc
    actor/bank_account.act.cc
    actor/bank_account.act.h
    ${bank_account_autogen_files})

add_dependencies (bank_account_sample_demo bank-account-autogen)