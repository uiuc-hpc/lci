target_include_directories(LCIXX PRIVATE .)
target_sources_relative(
  LCIXX
  PRIVATE
  util/log.cpp
  backend/backend.cpp
  backend/ofi/backend_ofi.cpp
  core/lcixx.cpp
  core/runtime.cpp)

target_include_directories(LCIXX PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
