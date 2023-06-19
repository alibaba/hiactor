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

if (NOT DEFINED PYTHON_EXECUTABLE)
  message ("-- No python executable specified, use 'python3' by default.")
  set (PYTHON_EXECUTABLE python3)
else ()
  message ("-- Use '${PYTHON_EXECUTABLE}' as python executable.")
endif()

if (NOT DEFINED PYTHON_PIP_EXECUTABLE)
  message ("-- No python-pip executable specified, use 'pip3' by default.")
  set (PYTHON_PIP_EXECUTABLE pip3)
  else ()
  message ("-- Use '${PYTHON_PIP_EXECUTABLE}' as python-pip executable.")
endif()

set (CLANG_LIB ${CURRENT_FILE_DIR}/clang/native/libclang.so)
add_custom_command (
  OUTPUT ${CLANG_LIB}
  COMMENT "Installing libclang ..."
  COMMAND ${PYTHON_PIP_EXECUTABLE} install -t ${CURRENT_FILE_DIR} --upgrade libclang==14.0.6
  VERBATIM)
if (NOT TARGET installed_libclang)
  add_custom_target (installed_libclang DEPENDS ${CLANG_LIB})
endif()

function (join_paths joined_path first_path_segment)
  set (temp_path "${first_path_segment}")
  foreach (current_segment IN LISTS ARGN)
    if (NOT ("${current_segment}" STREQUAL ""))
      if (IS_ABSOLUTE "${current_segment}")
        set (temp_path "${current_segment}")
      else ()
        set (temp_path "${temp_path}/${current_segment}")
      endif ()
    endif ()
  endforeach ()
  set (${joined_path} "${temp_path}" PARENT_SCOPE)
endfunction()

function (hiactor_codegen target_name actor_codegen_files)
  cmake_parse_arguments (args ""
    "SOURCE_DIR;INCLUDE_PATHS" "" ${ARGN})

  set (CODEGEN_DIR ${args_SOURCE_DIR}/generated)
  set (${actor_codegen_files})
  file (GLOB_RECURSE ACTOR_ACTG_H ${args_SOURCE_DIR}/*.actg.h)
  foreach (HEADER ${ACTOR_ACTG_H})
    file (RELATIVE_PATH REL_PATH ${args_SOURCE_DIR} ${HEADER})
    get_filename_component (REL_DIR ${REL_PATH} DIRECTORY)
    get_filename_component (BASE_NAME ${HEADER} NAME_WE)
    join_paths (ACTG_CC_GEN_FILE ${CODEGEN_DIR} ${REL_DIR} ${BASE_NAME}.actg.autogen.cc)
    list (APPEND ${actor_codegen_files} ${ACTG_CC_GEN_FILE})
  endforeach ()
  file (GLOB_RECURSE ACTOR_ACT_H ${args_SOURCE_DIR}/*.act.h)
  foreach (HEADER ${ACTOR_ACT_H})
    file (RELATIVE_PATH REL_PATH ${args_SOURCE_DIR} ${HEADER})
    get_filename_component (REL_DIR ${REL_PATH} DIRECTORY)
    get_filename_component (BASE_NAME ${HEADER} NAME_WE)
    join_paths (ACT_REF_H_GEN_FILE ${CODEGEN_DIR} ${REL_DIR} ${BASE_NAME}_ref.act.autogen.h)
    join_paths (ACT_CC_GEN_FILE ${CODEGEN_DIR} ${REL_DIR} ${BASE_NAME}.act.autogen.cc)
    list (APPEND ${actor_codegen_files} ${ACT_REF_H_GEN_FILE})
    list (APPEND ${actor_codegen_files} ${ACT_CC_GEN_FILE})
  endforeach ()

  set_source_files_properties (${${actor_codegen_files}} PROPERTIES GENERATED TRUE)
  set (${actor_codegen_files} ${${actor_codegen_files}} PARENT_SCOPE)

  set (PYTHON_FILES
    ${CURRENT_FILE_DIR}/actor_codegen.py
    ${CURRENT_FILE_DIR}/generator.py
    ${CURRENT_FILE_DIR}/traverse.py
    ${CURRENT_FILE_DIR}/typedef.py
    ${CURRENT_FILE_DIR}/utility.py)
  add_custom_command (
    OUTPUT ${${actor_codegen_files}}
    DEPENDS ${PYTHON_FILES} ${ACTOR_ACT_H} ${ACTOR_ACTG_H}
    COMMENT "Generating for actor definition files ..."
    COMMAND ${PYTHON_EXECUTABLE} actor_codegen.py
      --source-dir ${args_SOURCE_DIR}
      --include-paths ${args_INCLUDE_PATHS}
    WORKING_DIRECTORY ${CURRENT_FILE_DIR}
    VERBATIM)
  add_custom_target (${target_name}
    DEPENDS ${${actor_codegen_files}})
  add_dependencies (${target_name}
    installed_libclang)

  include_directories (${args_SOURCE_DIR})
endfunction ()
