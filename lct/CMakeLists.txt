option(LCT_DEBUG "Enable LCT debug mode" OFF)
if(LCI_DEBUG)
  set(LCT_DEBUG ON)
endif()
set(LCT_CACHE_LINE
    64
    CACHE STRING "LCT: Size of cache line (bytes)")
if(LCI_CACHE_LINE)
  set(LCT_CACHE_LINE)
endif()
option(LCT_CONFIG_USE_ALIGNED_ALLOC "Enable memory alignment" ON)

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

target_include_directories(LCT PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
