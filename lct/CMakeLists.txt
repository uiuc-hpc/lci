option(LCTI_CONFIG_USE_ALIGNED_ALLOC "Enable memory alignment" ON)

add_subdirectory(api)
add_subdirectory(data_structure)

target_include_directories(LCT PRIVATE . api)
target_sources_relative(
  LCT
  PRIVATE
  lct.cpp
  log/logger.cpp
  pcounter/pcounter.cpp
  util/thread.cpp
  util/time.cpp
  util/string.cpp)

configure_file(lcti_config.hpp.in lcti_config.h @ONLY)
target_include_directories(LCT PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
