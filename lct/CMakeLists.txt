function(lct_option option type description default)
  # If the corresponding LCI_option is defined, just use it. Otherwise, use the
  # default value.
  if(LCI_${option})
    set(LCT_${option}
        ${LCI_${option}}
        CACHE ${type} ${description})
  else()
    set(LCT_${option}
        ${default}
        CACHE ${type} ${description})
  endif()
endfunction()

lct_option(DEBUG BOOL "Enable LCT debug mode" OFF)
lct_option(CACHE_LINE STRING "LCT: Size of cache line (bytes)" 64)
lct_option(CONFIG_USE_ALIGNED_ALLOC BOOL "Enable memory alignment" ON)

add_subdirectory(data_structure)
add_subdirectory(pmi)

target_include_directories(LCT PRIVATE . api)
target_sources_relative(
  LCT
  PRIVATE
  lct.cpp
  log/logger.cpp
  pcounter/pcounter.cpp
  args_parser/args_parser.cpp
  tbarrier/tbarrier.cpp
  util/thread.cpp
  util/time.cpp
  util/string.cpp
  util/io.cpp)

target_include_directories(LCT PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
