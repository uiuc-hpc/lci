target_include_directories(LCI PRIVATE .)
target_sources_relative(
  LCI
  PRIVATE
  binding/generated/binding.cpp
  util/log.cpp
  util/random.cpp
  monitor/performance_counter.cpp
  backend/backend.cpp
  matching_engine/matching_engine.cpp
  core/lci.cpp
  core/global.cpp
  core/packet_pool.cpp
  core/cq.cpp
  core/comp.cpp
  core/rhandler_registry.cpp
  core/runtime.cpp
  core/communicate.cpp
  core/memory.cpp
  core/progress.cpp)
if(LCI_BACKEND_ENABLE_OFI)
  target_sources_relative(LCI PRIVATE backend/ofi/backend_ofi.cpp)
endif()
if(LCI_BACKEND_ENABLE_IBV)
  target_sources_relative(LCI PRIVATE backend/ibv/backend_ibv.cpp
                          backend/ibv/backend_ibv_detail.cpp)
endif()

target_include_directories(LCI PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
add_subdirectory(binding)
