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

add_custom_target (tests)

find_package (Boost COMPONENTS unit_test_framework REQUIRED)

macro (hiactor_add_test name)
  set (args ${ARGN})

  cmake_parse_arguments (
    parsed_args
    ""
    ""
    "SOURCES"
    ${args})

  set (target ${name})
  add_executable (${target} ${parsed_args_SOURCES})

  target_include_directories (${target}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

  target_link_libraries (${target}
    PRIVATE hiactor_private
    PUBLIC 
    Boost::unit_test_framework
    )

  set_target_properties (${target}
    PROPERTIES
    OUTPUT_NAME ${name})

endmacro ()

hiactor_add_test (actor_test
    SOURCES
    actor_test.cc
    actor_harness.cc
    actor_harness.hh
)

hiactor_add_test (alien_pool_test
    SOURCES
    alien_pool_test.cc
)
  
hiactor_add_test (thread_pool_test
    SOURCES
    thread_pool_test.cc
)

hiactor_add_test (column_batch_test
    SOURCES
    column_batch_test.cc
)
