find_package(OpenMP REQUIRED)

option(LCI_BUILD_TESTS "Build tests by default" ON)
if(NOT LCI_BUILD_TESTS)
  set(EXCLUDE_FROM_ALL ON)
endif()

include_directories(./)
add_subdirectory(unit)
# add_subdirectory(gtests) add_subdirectory(loopback) add_subdirectory(pingpong)
# add_subdirectory(lcit)
