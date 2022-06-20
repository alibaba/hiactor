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

cmake_minimum_required (VERSION 3.13)

project (Hiactor
  VERSION 1.0
  LANGUAGES CXX)

set (CMAKE_CXX_STANDARD
  "17"
  CACHE
  STRING
  "C++ standard to build with.")

option (Hiactor_DEBUG
  "Enable debug mode"
  OFF)

option (Hiactor_TESTING
  "Enable hiactor tests"
  ON)

option (Hiactor_DEMOS
  "Enable hiactor demos"
  ON)

option (Hiactor_INSTALL
  "Install targets."
  ON)

option (Hiactor_GPU_ENABLE
  "Enable gpu device for hiactor, cuda environments required!"
  OFF)

option (Hiactor_CPU_STALL_REPORT
  "Enable reporting at cpu stall."
  OFF)

option (Hiactor_UNUSED_RESULT_ERROR
  "Make [[nodiscard]] violations an error (instead of a warning)."
  OFF)

set (Hiactor_CXX_FLAGS
  ""
  CACHE
  STRING
  "Semicolon-separated list of extra compilation flags for Hiactor itself.")

# When Hiactor is embedded with `add_subdirectory`, disable the non-library targets.
if (NOT (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR))
  set (Hiactor_DEMOS OFF)
  set (Hiactor_TESTING OFF)
  set (Hiactor_INSTALL OFF)
endif ()

set (Hiactor_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set (Hiactor_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

## Disable io uring of seastar now
set (Seastar_IO_URING OFF)

## Set seastar dirs
set (Hiactor_BUILD_SEASTAR_SOURCE_DIR ${Hiactor_BINARY_DIR}/seastar)
set (Hiactor_BUILD_SEASTAR_BINARY_DIR ${Hiactor_BINARY_DIR}/build/seastar)

## Copy seastar repo and apply patch
if (NOT EXISTS ${Hiactor_BUILD_SEASTAR_SOURCE_DIR})
  message ("Copying seastar source repo into build dir ...")
  execute_process (
    COMMAND git clone -q ${Hiactor_SOURCE_DIR}/seastar/seastar
    WORKING_DIRECTORY ${Hiactor_BINARY_DIR})

  message ("Applying seastar patch from hiactor ...")
  execute_process (
    COMMAND git am -q ${Hiactor_SOURCE_DIR}/seastar/patch/hiactor-hackings.patch
    WORKING_DIRECTORY ${Hiactor_BUILD_SEASTAR_SOURCE_DIR})
endif ()

add_subdirectory (${Hiactor_BINARY_DIR}/seastar)

if (Hiactor_GPU_ENABLE)
  # add compiling source files
  set (Hiactor_GPU_POOL_SOURCES
    include/hiactor/core/gpu_resource_pool.hh
    src/core/gpu_resource_pool.cc)
  add_definitions (-DHIACTOR_GPU_ENABLE)
  # build cuda kernels
  find_package (CUDA 9.0 REQUIRED)
  set (CUDA_NVCC_FLAGS "-std=c++14 --compiler-options -fPIC") # nvcc flags
  if (Hiactor_DEBUG)
    set (CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS}; -O1; -G; -g; -line-info)
  else ()
    set (CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS}; -O2)
  endif ()
  cuda_add_library (cuda_kernels STATIC
    include/hiactor/cuda/device.h
    src/cuda/device.cu)
endif ()

if (Hiactor_CPU_STALL_REPORT)
  add_definitions (-DHIACTOR_CPU_STALL_REPORT)
endif ()

add_library (hiactor STATIC
  ${Hiactor_GPU_POOL_SOURCES}
  include/hiactor/core/actor-app.hh
  include/hiactor/core/actor-template.hh
  include/hiactor/core/actor_client.hh
  include/hiactor/core/actor_core.hh
  include/hiactor/core/actor_factory.hh
  include/hiactor/core/actor_message.hh
  include/hiactor/core/actor_pollfn.hh
  include/hiactor/core/actor_scheduling.hh
  include/hiactor/core/actor_timer.hh
  include/hiactor/core/alien_thread_pool.hh
  include/hiactor/core/cb_table.hh
  include/hiactor/core/column_batch.hh
  include/hiactor/core/continuation.hh
  include/hiactor/core/coordinator.hh
  include/hiactor/core/coordinator_worker.hh
  include/hiactor/core/dynamic-queue.hh
  include/hiactor/core/exception.hh
  include/hiactor/core/local_channel.hh
  include/hiactor/core/message_reclaimer.hh
  include/hiactor/core/message_tag.hh
  include/hiactor/core/promise_manager.hh
  include/hiactor/core/reference_base.hh
  include/hiactor/core/root_actor_group.hh
  include/hiactor/core/shard-config.hh
  include/hiactor/core/thread_resource_pool.hh
  include/hiactor/net/connection.hh
  include/hiactor/net/defs.hh
  include/hiactor/net/entry.hh
  include/hiactor/net/lba_policy.hh
  include/hiactor/net/network_channel.hh
  include/hiactor/net/network_config.hh
  include/hiactor/net/network_io.hh
  include/hiactor/net/serializable_queue.hh
  include/hiactor/util/common-utils.hh
  include/hiactor/util/data_type.hh
  include/hiactor/util/machine_info.hh
  include/hiactor/util/unaligned_int.hh
  include/hiactor/testing/test_case.hh
  src/core/actor-app.cc
  src/core/actor_core.cc
  src/core/actor_factory.cc
  src/core/actor-template.cc
  src/core/actor_message.cc
  src/core/actor_timer.cc
  src/core/alien_thread_pool.cc
  src/core/coordinator.cc
  src/core/coordinator_worker.cc
  src/core/local_channel.cc
  src/core/root_actor_group.cc
  src/core/thread_resource_pool.cc
  src/net/connection.cc
  src/net/network_channel.cc
  src/net/network_io.cc)

add_library (Hiactor::hiactor ALIAS hiactor)

target_include_directories (hiactor
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${Hiactor_SOURCE_DIR}/include>
  PRIVATE
    ${Hiactor_SOURCE_DIR}/src)

set (Hiactor_PRIVATE_CXX_FLAGS
  -fvisibility=hidden
  -UNDEBUG
  -Wall
  -Werror
  -Wno-array-bounds
  -Wno-error=deprecated-declarations
  -Wno-attributes)

if (Hiactor_CXX_FLAGS)
  list (APPEND Hiactor_PRIVATE_CXX_FLAGS ${Hiactor_CXX_FLAGS})
endif ()

target_compile_options (hiactor
  PRIVATE ${Hiactor_PRIVATE_CXX_FLAGS})

if (Hiactor_UNUSED_RESULT_ERROR)
  target_compile_options (hiactor
    PUBLIC
      -Werror=unused-result
      -Werror=unused-variable)
else ()
  target_compile_options (hiactor
    PUBLIC
      -Wno-unused-result
      -Wno-unused-variable)
endif ()

if (Hiactor_DEBUG)
  add_definitions (-DHIACTOR_DEBUG)
  set (Hiactor_MODE_FLAGS -DDEBUG -g)
else ()
  set (Hiactor_MODE_FLAGS -DNDEBUG -O2)
endif ()

target_compile_options (hiactor
  PUBLIC ${Hiactor_MODE_FLAGS})

target_compile_features (hiactor
  PUBLIC
    cxx_std_${CMAKE_CXX_STANDARD})

set_target_properties (hiactor
  PROPERTIES
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_EXTENSIONS ON)

target_link_libraries (hiactor
  PUBLIC
    seastar
  PRIVATE
    stdc++fs)

if (Hiactor_GPU_ENABLE)
  target_link_libraries (hiactor PUBLIC cuda_kernels)
endif ()

## hiactor private interface
add_library (hiactor_private INTERFACE)

target_compile_options (hiactor_private
  INTERFACE ${Hiactor_PRIVATE_CXX_FLAGS})

target_link_libraries (hiactor_private
  INTERFACE hiactor)

include (${Hiactor_SOURCE_DIR}/codegen/ActorAutoGen.cmake)

## hiactor demos
if (Hiactor_DEMOS)
  add_subdirectory (demos)
endif ()

## hiactor tests
if (Hiactor_TESTING)
  add_subdirectory (tests)
  enable_testing ()
  add_test (actor_test tests/actor_test -- -c4)
  add_test (alien_pool_test tests/alien_pool_test -- -c4)
  add_test (thread_pool_test tests/thread_pool_test -- --open-thread-resource-pool=true)
  add_test (column_batch_test tests/column_batch_test)
endif ()

## install targets
if (Hiactor_INSTALL)
  include (GNUInstallDirs)
  include (CMakePackageConfigHelpers)
  set (install_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/Hiactor)

  # install include files
  install (
    DIRECTORY ${Hiactor_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  install (
    DIRECTORY ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  set (Hiactor_BUILD_SEASTAR_GEN_BINARY_DIR ${Hiactor_BUILD_SEASTAR_BINARY_DIR}/gen)
  install (
    DIRECTORY ${Hiactor_BUILD_SEASTAR_GEN_BINARY_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # install actor codegen tool
  install (
    FILES
      ${Hiactor_SOURCE_DIR}/codegen/__init__.py
      ${Hiactor_SOURCE_DIR}/codegen/actor_codegen.py
      ${Hiactor_SOURCE_DIR}/codegen/generator.py
      ${Hiactor_SOURCE_DIR}/codegen/traverse.py
      ${Hiactor_SOURCE_DIR}/codegen/typedef.py
      ${Hiactor_SOURCE_DIR}/codegen/utility.py
      ${Hiactor_SOURCE_DIR}/codegen/ActorAutoGen.cmake
    DESTINATION ${CMAKE_INSTALL_BINDIR}/hiactor_codegen)

  # install seastar binaries
  install (
    PROGRAMS ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/scripts/seastar-json2code.py
    DESTINATION ${CMAKE_INSTALL_BINDIR})

  # install targets
  install (
    TARGETS
      hiactor
      seastar
    EXPORT hiactor-export
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  if (Hiactor_GPU_ENABLE)
    install (
      TARGETS cuda_kernels
      EXPORT hiactor-export
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endif ()

  install (
    EXPORT hiactor-export
    FILE HiactorTargets.cmake
    NAMESPACE Hiactor::
    DESTINATION ${install_cmake_dir})

  write_basic_package_version_file (
    ${Hiactor_BINARY_DIR}/HiactorConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

  configure_package_config_file (
    ${Hiactor_SOURCE_DIR}/cmake/HiactorConfig.cmake.in
    ${Hiactor_BINARY_DIR}/HiactorConfig.cmake
    INSTALL_DESTINATION ${install_cmake_dir})

  install (
    FILES
      ${Hiactor_BINARY_DIR}/HiactorConfig.cmake
      ${Hiactor_BINARY_DIR}/HiactorConfigVersion.cmake
    DESTINATION ${install_cmake_dir})

  install (
    FILES
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/FindConcepts.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/FindGnuTLS.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/FindLinuxMembarrier.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/FindProtobuf.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/FindSanitizers.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/FindStdAtomic.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/FindStdFilesystem.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findc-ares.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findcryptopp.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Finddpdk.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findhwloc.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findlksctp-tools.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findlz4.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findnumactl.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findragel.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findrt.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/Findyaml-cpp.cmake
      ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/SeastarDependencies.cmake
    DESTINATION ${install_cmake_dir})

  install (
    DIRECTORY ${Hiactor_BUILD_SEASTAR_SOURCE_DIR}/cmake/code_tests
    DESTINATION ${install_cmake_dir})

  # Export targets from the build tree for the user package registry.

  export (
    EXPORT hiactor-export
    FILE ${Hiactor_BINARY_DIR}/HiactorTargets.cmake
    NAMESPACE Hiactor::)

  export (PACKAGE Hiactor)

  # Packaging.

  set (CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set (CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set (CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

  include (CPack)
endif ()
