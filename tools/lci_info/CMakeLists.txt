# Collect all cache variables
get_cmake_property(_vars CACHE_VARIABLES)

set(LCI_CMAKE_CACHE_STRING "")
foreach(var ${_vars})
  set(LCI_CMAKE_CACHE_STRING "${LCI_CMAKE_CACHE_STRING}${var} = ${${var}}\n")
endforeach()

# Escape quotes and newlines for C/C++ string literal
string(REPLACE "\"" "\\\"" LCI_CMAKE_CACHE_STRING "${LCI_CMAKE_CACHE_STRING}")
string(REPLACE "\n" "\\n\"\n\"" LCI_CMAKE_CACHE_STRING
               "${LCI_CMAKE_CACHE_STRING}")

# Write to header
file(
  WRITE "${CMAKE_CURRENT_BINARY_DIR}/lci_info_cache.hpp"
  "#ifndef LCI_INFO_CACHE_H\n\n" "#define LCI_INFO_CACHE_H\n\n"
  "static const char* LCI_CMAKE_CACHE_INFO = \"${LCI_CMAKE_CACHE_STRING}\";\n"
  "#endif // LCI_INFO_CACHE_H\n")

add_lci_executable(info lci_info.cpp)
target_include_directories(info PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
