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

set (CURRENT_FILE_DIR ${CMAKE_CURRENT_LIST_DIR})

set (CLANG_LIB ${CURRENT_FILE_DIR}/clang/native/libclang.so)
add_custom_command (
  OUTPUT ${CLANG_LIB}
  COMMENT "Installing libclang ..."
  COMMAND pip3 install -t ${CURRENT_FILE_DIR} --upgrade libclang
  VERBATIM)
add_custom_target (installed_libclang
  DEPENDS ${CLANG_LIB})

function (hiactor_codegen target_name actor_codegen_files)
  cmake_parse_arguments (args ""
    "ACTOR_SOURCE_DIR;HIACTOR_INCLUDE;SYSTEM_INCLUDE;USER_INCLUDE" "" ${ARGN})

  set (CODEGEN_DIR ${args_ACTOR_SOURCE_DIR}/generated)
  set (${actor_codegen_files})
  file (GLOB_RECURSE ACTOR_ACTG_H ${args_ACTOR_SOURCE_DIR}/*.actg.h)
  foreach (HEADER ${ACTOR_ACTG_H})
    get_filename_component (BASE_NAME ${HEADER} NAME_WE)
    list (APPEND ${actor_codegen_files} "${CODEGEN_DIR}/${BASE_NAME}.actg.autogen.cc")
  endforeach ()
  file (GLOB_RECURSE ACTOR_ACT_H ${args_ACTOR_SOURCE_DIR}/*.act.h)
  foreach (HEADER ${ACTOR_ACT_H})
    get_filename_component (BASE_NAME ${HEADER} NAME_WE)
    list (APPEND ${actor_codegen_files} "${CODEGEN_DIR}/${BASE_NAME}_ref.act.autogen.h")
    list (APPEND ${actor_codegen_files} "${CODEGEN_DIR}/${BASE_NAME}.act.autogen.cc")
  endforeach ()

  set_source_files_properties (${${actor_codegen_files}} PROPERTIES GENERATED TRUE)
  set (${actor_codegen_files} ${${actor_codegen_files}} PARENT_SCOPE)

  add_custom_command (
    OUTPUT ${${actor_codegen_files}}
    DEPENDS ${ACTOR_ACT_H} ${ACTOR_ACTG_H}
    COMMENT "Generating for actor definition files ..."
    COMMAND python3 actor_codegen.py
      --actor-source-dir=${args_ACTOR_SOURCE_DIR}
      --hiactor-include=${args_HIACTOR_INCLUDE}
      --system-include=${args_SYSTEM_INCLUDE}
      --user-include=${args_USER_INCLUDE}
    WORKING_DIRECTORY ${CURRENT_FILE_DIR}
    VERBATIM)
  add_custom_target (${target_name}
    DEPENDS ${${actor_codegen_files}})
  add_dependencies (${target_name} installed_libclang)

  include_directories (${args_ACTOR_SOURCE_DIR})
endfunction ()
