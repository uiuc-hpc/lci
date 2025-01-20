target_include_directories(LCIXX PRIVATE .)
target_sources_relative(
  LCIXX
  PRIVATE
  binding/generated/binding.cpp
  util/log.cpp
  util/random.cpp
  data_structure/cq/cq.cpp
  backend/backend.cpp
  backend/ofi/backend_ofi.cpp
  core/lcixx.cpp
  core/global.cpp
  core/comp.cpp
  core/runtime.cpp)

target_include_directories(LCIXX PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
add_subdirectory(binding)
