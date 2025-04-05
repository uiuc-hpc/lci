# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

target_include_directories(LCI PRIVATE .)

add_subdirectory(binding)

target_sources_relative(
  LCI
  PRIVATE
  binding/generated/binding.cpp
  util/log.cpp
  util/random.cpp
  monitor/performance_counter.cpp
  bootstrap/bootstrap.cpp
  network/network.cpp
  matching_engine/matching_engine.cpp
  core/lci.cpp
  global/global.cpp
  packet_pool/packet_pool.cpp
  comp/comp.cpp
  rhandler_registry/rhandler_registry.cpp
  runtime/runtime.cpp
  core/communicate.cpp
  core/progress.cpp
  collective/collective.cpp)

if(LCI_BACKEND_ENABLE_OFI)
  target_sources_relative(LCI PRIVATE network/ofi/backend_ofi.cpp)
endif()
if(LCI_BACKEND_ENABLE_IBV)
  target_sources_relative(LCI PRIVATE network/ibv/backend_ibv.cpp
                          network/ibv/backend_ibv_detail.cpp)
endif()

target_include_directories(LCI PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
