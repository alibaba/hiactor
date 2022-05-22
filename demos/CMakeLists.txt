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

# Logical target for all demos.
add_custom_target (demos)

macro (hiactor_add_demo name)
  set (args ${ARGN})

  cmake_parse_arguments (
    parsed_args
    ""
    ""
    "SOURCES"
    ${args})

  set (target ${name}_demo)
  add_executable (${target} ${parsed_args_SOURCES})

  target_include_directories (${target}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

  target_link_libraries (${target}
    PRIVATE hiactor_private)

  set_target_properties (${target}
    PROPERTIES
    OUTPUT_NAME ${name})

  add_dependencies (demos ${target})
endmacro ()

add_subdirectory (actor_samples)
add_subdirectory (bank_account)
add_subdirectory (thread_pool_sample)
add_subdirectory (clinic_schedule)

# gpu_resource_pool_sample is disable by default when CUDA environments is missing.
if (Hiactor_GPU_ENABLE)
  add_subdirectory (gpu_pool_sample)
endif()
