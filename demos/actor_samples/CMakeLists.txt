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

hiactor_codegen (actor-autogen actor_sample_gen_files
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  INCLUDE_PATHS ${CMAKE_SOURCE_DIR}/include,${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/include,${CMAKE_CURRENT_SOURCE_DIR})

hiactor_add_demo (simple_actor_sample
  SOURCES
    simple_actor_sample.cc
    actor/item_store.h
    actor/items.act.h
    actor/items.act.cc
    generated/items_ref.act.autogen.h
    generated/items.act.autogen.cc)

hiactor_add_demo (actor_scheduling_sample
  SOURCES
    actor_scheduling_sample.cc
    actor/item_groups.actg.h
    actor/item_store.h
    actor/items.act.h
    actor/items.act.cc
    ${actor_sample_gen_files})

add_dependencies (simple_actor_sample_demo actor-autogen)
add_dependencies (actor_scheduling_sample_demo actor-autogen)
