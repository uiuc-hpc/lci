@page add_lci Add LCI to your project

This tutorial will show you how to integrate LCI to your project.

## With CMake
The following CMake code will add LCI to your project. It will first try to find LCI on your system. If it is not found, it will download and build LCI from GitHub.

```cmake
# Try to find LCI externally
find_package(
  LCI
  CONFIG
  PATH_SUFFIXES
  lib/cmake
  lib64/cmake)
if(NOT LCI_FOUND)
  message(STATUS "Existing LCI installation not found. Try FetchContent.")
  include(FetchContent)
  FetchContent_Declare(
    lci
    GIT_REPOSITORY https://github.com/uiuc-hpc/lci.git
    GIT_TAG master)
  FetchContent_MakeAvailable(lci)
endif()

# Link LCI to your target
target_link_libraries(your_target PRIVATE LCI::lci)
```