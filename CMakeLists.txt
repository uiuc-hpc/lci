cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(
    LCI
    VERSION 2.0.0
    DESCRIPTION "Lightweight Communication Interface (LCI)"
    HOMEPAGE_URL "https://github.com/uiuc-hpc/lci")
  enable_testing()
endif()

cmake_policy(SET CMP0079 NEW)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(TargetSourcesRelative)
include(AddLCI)

# ##############################################################################
# General Options
# ##############################################################################
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# ##############################################################################
# LCT options
# ##############################################################################
option(LCI_WITH_LCT_ONLY
       "Only Build the Lightweight Communication Tools (LCT) Library" OFF)

add_library(LCT)
set_target_properties(LCT PROPERTIES CXX_VISIBILITY_PRESET hidden CXX_STANDARD
                                                                  17)
set_target_properties(LCT PROPERTIES OUTPUT_NAME lct)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_include_directories(LCT PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LCT PUBLIC Threads::Threads)
add_subdirectory(lct)

if(NOT LCI_WITH_LCT_ONLY)
  # ############################################################################
  # Add the actual LCI library
  # ############################################################################

  add_library(LCI)
  set_target_properties(LCI PROPERTIES CXX_STANDARD 11 CXX_EXTENSIONS ON)
  target_compile_definitions(LCI PRIVATE _GNU_SOURCE)
  target_link_libraries(LCI PUBLIC LCT)
  set_target_properties(LCI PROPERTIES OUTPUT_NAME lci)
  target_compile_options(LCI PRIVATE -Wall -Wextra -Wpedantic -Werror)

  # ############################################################################
  # What parts of LCI to build
  # ############################################################################
  option(LCI_WITH_EXAMPLES "Build LCI examples" ON)
  option(LCI_WITH_TESTS "Build LCI tests" ON)
  option(LCI_WITH_BENCHMARKS "Build LCI benchmarks" ON)
  option(LCI_WITH_DOC "Build LCI documentation" ON)

  # ############################################################################
  # Figure out which network backend to use
  # ############################################################################
  set(LCI_NETWORK_BACKENDS_DEFAULT ibv ofi)

  # check the alias LCI_SERVER
  if(LCI_SERVER)
    message(
      WARNING "LCI_SERVER is deprecated. Use LCI_NETWORK_BACKENDS instead.")
    set(LCI_NETWORK_BACKENDS_DEFAULT ${LCI_SERVER})
  endif()

  set(LCI_NETWORK_BACKENDS
      ${LCI_NETWORK_BACKENDS_DEFAULT}
      CACHE STRING "Network backends to build.")
  string(REPLACE "," ";" LCI_NETWORK_BACKENDS "${LCI_NETWORK_BACKENDS}")

  set(LCI_OFI_PROVIDER_HINT_DEFAULT
      ""
      CACHE
        STRING
        "If using the ofi(libfabric) backend, provide a hint for the provider to use"
  )

  set(LCI_NETWORK_BACKENDS_ENABLED "")
  message(STATUS "Looking for network backends ${LCI_NETWORK_BACKENDS_ENABLED}")
  foreach(BACKEND ${LCI_NETWORK_BACKENDS})
    message(STATUS "Looking for ${BACKEND} backend")
    string(TOUPPER ${BACKEND} BACKEND)
    find_package(${BACKEND})

    if(NOT ${BACKEND}_FOUND)
      message(STATUS "Did not find ${BACKEND} backend")
      continue()
    endif()

    message(STATUS "Found ${BACKEND} backend")
    list(APPEND LCI_NETWORK_BACKENDS_ENABLED ${BACKEND})
    set(LCI_BACKEND_ENABLE_${BACKEND} ON)

    if(BACKEND STREQUAL UCX)
      target_link_libraries(LCI PUBLIC ucx::ucp)
    else()
      target_link_libraries(LCI PUBLIC ${BACKEND}::${BACKEND})
    endif()
  endforeach()

  if(NOT LCI_NETWORK_BACKENDS_ENABLED)
    message(FATAL_ERROR "Cannot find any backends. Give up!")
  endif()

  set(LCI_NETWORK_BACKENDS_ENABLED
      ${LCI_NETWORK_BACKENDS_ENABLED}
      CACHE
        STRING
        "Used as an output variable if LCI is included in a larger project via FetchContent"
        FORCE)

  # ############################################################################
  # LCI Optimization Options
  # ############################################################################

  # debug
  option(LCI_DEBUG "LCI Debug Mode" OFF)

  # performance counter
  option(LCI_USE_PERFORMANCE_COUNTER "Use performance counter" ${LCI_DEBUG})

  # cache line
  set(LCI_CACHE_LINE
      64
      CACHE STRING "Size of cache line (bytes)")

  # TCMalloc
  option(LCI_USE_TCMALLOC "Use TCMalloc for memory allocation" ON)
  if(LCI_USE_TCMALLOC)
    find_package(TCMalloc)
    if(NOT TCMALLOC_FOUND)
      message(
        FATAL_ERROR
          "TCMalloc is not found; Please either install it or pass -DLCI_USE_TCMALLOC=OFF"
      )
    endif()
    target_link_libraries(LCI PRIVATE ${Tcmalloc_LIBRARIES})
  endif()

  # memory allocation with alignment
  option(LCI_CONFIG_USE_ALIGNED_ALLOC "Enable memory alignment" ON)

  # registration cache
  set(LCI_COMPILE_REG_CACHE_DEFAULT ON)
  if(LCI_BACKEND_ENABLE_UCX)
    set(LCI_COMPILE_REG_CACHE_DEFAULT OFF)
  endif()
  set(LCI_COMPILE_REG_CACHE
      ${LCI_COMPILE_REG_CACHE_DEFAULT}
      CACHE STRING "Whether to compile the registration cache code")
  set(LCI_USE_REG_CACHE_DEFAULT
      ${LCI_USE_SERVER_IBV}
      CACHE STRING "Whether to use registration cache")

  # packet size/number
  set(LCI_PACKET_SIZE_DEFAULT
      8192
      CACHE STRING "Size of packet")
  set(LCI_PACKET_NUM_DEFAULT
      65536
      CACHE STRING "Number of packets")

  # maximum number of sends, recvs, and cqes for backend
  set(LCI_BACKEND_MAX_SENDS_DEFAULT
      64
      CACHE STRING "Max posted sends")
  set(LCI_BACKEND_MAX_RECVS_DEFAULT
      4096
      CACHE STRING "Max posted recvs")
  set(LCI_BACKEND_MAX_CQES_DEFAULT
      65536
      CACHE STRING "Max posted cqes")
  set(LCI_BACKEND_MAX_POLLS
      32
      CACHE STRING "Max number of cqes to poll at one time")

  # matching table backend
  set(LCI_USE_MT_BACKEND_DEFAULT
      "hashqueue"
      CACHE STRING "The default matching table backend to use.")
  set_property(CACHE LCI_USE_MT_BACKEND_DEFAULT PROPERTY STRINGS hash queue
                                                         hashqueue)

  # ibv thread domain
  option(LCI_USE_IBV_TD_DEFAULT
         "Try to lock the IBV queue pair before access it." ON)

  # progress endpoint
  option(LCI_USE_PRG_ENDPOINT_DEFAULT
         "Enable the progress specific network endpoint by default." ON)

  # optimize for native
  include(CheckCCompilerFlag)
  check_c_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
  option(LCI_OPTIMIZE_FOR_NATIVE "Build with -march=native"
         ${COMPILER_SUPPORTS_MARCH_NATIVE})
  if(LCI_OPTIMIZE_FOR_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    else()
      message(
        FATAL_ERROR
          "LCI_OPTIMIZE_FOR_NATIVE is set explicitly but the C compiler doesn't support -march=native"
      )
    endif()
  endif()

  # avx
  option(LCI_USE_AVX "Use GCC vector extension for the immediate field" OFF)
  if(LCI_USE_AVX)
    check_c_compiler_flag("-mavx" COMPILER_SUPPORTS_MAVX)
    if(NOT COMPILER_SUPPORTS_MAVX)
      message(
        FATAL_ERROR
          "COMPILER_SUPPORTS_MAVX is set explicitly but the C compiler doesn't support -mavx"
      )
    endif()
  endif()
  if(LCI_USE_AVX)
    target_compile_options(LCI PUBLIC -mavx)
  endif()

  # rendezvous protocol
  set(LCI_USE_RDV_PROTOCOL_DEFAULT
      writeimm
      CACHE STRING "The default rendezvous protocol to use (write, writeimm).")
  set_property(CACHE LCI_USE_RDV_PROTOCOL_DEFAULT PROPERTY STRINGS write
                                                           writeimm)

  # max single message size
  set(LCI_USE_MAX_SINGLE_MESSAGE_SIZE_DEFAULT
      0x7FFFFFFF
      CACHE STRING "Default single low-level message max size")

  # papi
  find_package(PAPI)
  option(LCI_USE_PAPI "Use PAPI to collect hardware counters" ${PAPI_FOUND})
  if(LCI_USE_PAPI)
    if(NOT PAPI_FOUND)
      message(FATAL_ERROR "LCI_USE_PAPI is enabled but papi is not found")
    endif()
    target_link_libraries(LCI PRIVATE Papi::papi)
  endif()

  # ############################################################################
  mark_as_advanced(
    LCI_CONFIG_USE_ALIGNED_ALLOC
    LCI_PACKET_SIZE_DEFAULT
    LCI_BACKEND_MAX_SENDS_DEFAULT
    LCI_BACKEND_MAX_RECVS_DEFAULT
    LCI_BACKEND_MAX_CQES_DEFAULT
    LCI_PACKET_NUM_DEFAULT
    LCI_CACHE_LINE
    LCI_USE_RDV_PROTOCOL_DEFAULT
    LCI_USE_MAX_SINGLE_MESSAGE_SIZE_DEFAULT)

  # ############################################################################
  # LCI Testing related options
  # ############################################################################
  set(LCI_USE_CTEST_LAUNCHER
      ${CMAKE_CURRENT_SOURCE_DIR}/lcrun
      CACHE STRING "exective to be used in ctest")
  set(LCI_USE_CTEST_ARGS
      ""
      CACHE STRING "arguments to be used in ctest")

  # ############################################################################
  # Add Subdirectories
  # ############################################################################
  add_subdirectory(src)
  add_subdirectory(third_party)

  if(LCI_WITH_EXAMPLES)
    add_subdirectory(examples)
  endif()
  if(LCI_WITH_BENCHMARKS)
    add_subdirectory(benchmarks)
  endif()
  if(LCI_WITH_TESTS)
    add_subdirectory(tests)
  endif()
  if(LCI_WITH_DOC)
    add_subdirectory(doc)
  endif()
endif() # if (NOT LCI_WITH_LCT_ONLY)

# ##############################################################################
# Install
# ##############################################################################
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(PKGCONFIG_REQUIRES_PRIVATE ${Fabric_${FABRIC}_PC_Requires})
  set(PKGCONFIG_LIBS_PRIVATE ${Fabric_${FABRIC}_PC_Libs})
  configure_file(liblci.pc.in liblci.pc @ONLY)

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LCIConfigVersion.cmake"
    COMPATIBILITY ExactVersion)
  configure_package_config_file(
    LCIConfig.cmake.in LCIConfig.cmake
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR)
  install(
    TARGETS LCT
    EXPORT LCITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(
    DIRECTORY lct/api/ ${CMAKE_CURRENT_BINARY_DIR}/lct/api/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h")

  if(NOT LCI_WITH_LCT_ONLY)
    install(
      TARGETS LCI
      EXPORT LCITargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    if(TARGET lci-ucx)
      install(
        TARGETS lci-ucx
        EXPORT LCITargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    install(
      DIRECTORY src/api/ ${CMAKE_CURRENT_BINARY_DIR}/src/api/
                src/binding/generated/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING
      PATTERN "*.hpp")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblci.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
    install(
      DIRECTORY cmake_modules/
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI
      FILES_MATCHING
      PATTERN "*.cmake")
    install(PROGRAMS lcrun DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()
  install(
    EXPORT LCITargets
    FILE LCITargets.cmake
    NAMESPACE LCI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LCIConfig.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/LCIConfigVersion.cmake"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
endif()
