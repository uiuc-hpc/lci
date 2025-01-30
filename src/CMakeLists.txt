target_include_directories(LCIXX PRIVATE .)
target_sources_relative(
  LCIXX
  PRIVATE
  binding/generated/binding.cpp
  util/log.cpp
  util/random.cpp
  monitor/performance_counter.cpp
  backend/backend.cpp
  backend/ofi/backend_ofi.cpp
  core/lcixx.cpp
  core/global.cpp
  core/packet_pool.cpp
  core/cq.cpp
  core/comp.cpp
  core/rcomp_registry.cpp
  core/runtime.cpp
  core/communicate.cpp
  core/progress.cpp)

target_include_directories(LCIXX PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
add_subdirectory(binding)
