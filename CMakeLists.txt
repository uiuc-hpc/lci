cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(
    LCIXX
    VERSION 0.1.0
    DESCRIPTION "Lightweight Communication Interface++ (LCI++)"
    HOMEPAGE_URL "https://github.com/uiuc-hpc/lcixx")
  enable_testing()
endif()

cmake_policy(SET CMP0079 NEW)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(TargetSourcesRelative)
include(AddLCIXX)

# ##############################################################################
# General Options
# ##############################################################################
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# ##############################################################################
# LCT options
# ##############################################################################
option(LCIXX_WITH_LCT_ONLY
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

if(NOT LCIXX_WITH_LCT_ONLY)
  # ############################################################################
  # Add the actual LCIXX library
  # ############################################################################

  add_library(LCIXX)
  set_target_properties(LCIXX PROPERTIES CXX_STANDARD 11 CXX_EXTENSIONS ON)
  target_compile_definitions(LCIXX PRIVATE _GNU_SOURCE)
  target_link_libraries(LCIXX PUBLIC LCT)
  set_target_properties(LCIXX PROPERTIES OUTPUT_NAME lcixx)

  # ############################################################################
  # What parts of LCIXX to build
  # ############################################################################
  option(LCIXX_WITH_EXAMPLES "Build LCIXX examples" ON)
  option(LCIXX_WITH_TESTS "Build LCIXX tests" ON)
  option(LCIXX_WITH_BENCHMARKS "Build LCIXX benchmarks" ON)
  option(LCIXX_WITH_DOC "Build LCIXX documentation" ON)

  # ############################################################################
  # Figure out which network backend to use
  # ############################################################################
  set(LCIXX_NETWORK_BACKENDS
      ibv ofi ucx
      CACHE STRING "Network backends to build.")

  set(LCIXX_OFI_PROVIDER_HINT_DEFAULT
      ""
      CACHE
        STRING
        "If using the ofi(libfabric) backend, provide a hint for the provider to use"
  )

  set(LCIXX_NETWORK_BACKENDS_ENABLED "")
  message(
    STATUS "Looking for network backends ${LCIXX_NETWORK_BACKENDS_ENABLED}")
  foreach(BACKEND ${LCIXX_NETWORK_BACKENDS})
    message(STATUS "Looking for ${BACKEND} backend")
    string(TOUPPER ${BACKEND} BACKEND)
    find_package(${BACKEND})

    if(NOT ${BACKEND}_FOUND)
      message(STATUS "Did not find ${BACKEND} backend")
      continue()
    endif()

    message(STATUS "Found ${BACKEND} backend")
    list(APPEND LCIXX_NETWORK_BACKENDS_ENABLED ${BACKEND})
    set(LCIXX_BACKEND_ENABLE_${BACKEND} ON)

    if(BACKEND STREQUAL UCX)
      target_link_libraries(LCIXX PUBLIC ucx::ucp)
    else()
      target_link_libraries(LCIXX PUBLIC ${BACKEND}::${BACKEND})
    endif()
  endforeach()

  if(NOT LCIXX_NETWORK_BACKENDS_ENABLED)
    message(FATAL_ERROR "Cannot find any backends. Give up!")
  endif()

  set(LCIXX_NETWORK_BACKENDS_ENABLED
      ${LCIXX_NETWORK_BACKENDS_ENABLED}
      CACHE
        STRING
        "Used as an output variable if LCIXX is included in a larger project via FetchContent"
        FORCE)

  # ############################################################################
  # LCIXX Optimization Options
  # ############################################################################

  # debug
  option(LCIXX_DEBUG "LCIXX Debug Mode" OFF)

  # performance counter
  option(LCIXX_USE_PERFORMANCE_COUNTER "Use performance counter" OFF)

  # cache line
  set(LCIXX_CACHE_LINE
      64
      CACHE STRING "Size of cache line (bytes)")

  # memory allocation with alignment
  option(LCIXX_CONFIG_USE_ALIGNED_ALLOC "Enable memory alignment" ON)

  # registration cache
  set(LCIXX_COMPILE_DREG_DEFAULT ON)
  if(LCIXX_BACKEND_ENABLE_UCX)
    set(LCIXX_COMPILE_DREG_DEFAULT OFF)
  endif()
  set(LCIXX_COMPILE_DREG
      ${LCIXX_COMPILE_DREG_DEFAULT}
      CACHE STRING "Whether to compile the registration cache code")
  set(LCIXX_USE_DREG_DEFAULT
      ${LCIXX_USE_SERVER_IBV}
      CACHE STRING "Whether to use registration cache")

  # packet size/number
  set(LCIXX_PACKET_SIZE_DEFAULT
      12288
      CACHE STRING "Size of packet")
  set(LCIXX_PACKET_NUM_DEFAULT
      8192
      CACHE STRING "Number of packets")

  # maximum number of sends, recvs, and cqes for backend
  set(LCIXX_BACKEND_MAX_SENDS_DEFAULT
      64
      CACHE STRING "Max posted sends")
  set(LCIXX_BACKEND_MAX_RECVS_DEFAULT
      1024
      CACHE STRING "Max posted recvs")
  set(LCIXX_BACKEND_MAX_CQES_DEFAULT
      65536
      CACHE STRING "Max posted cqes")

  # matching table backend
  set(LCIXX_USE_MT_BACKEND_DEFAULT
      "hashqueue"
      CACHE STRING "The default matching table backend to use.")
  set_property(CACHE LCIXX_USE_MT_BACKEND_DEFAULT PROPERTY STRINGS hash queue
                                                           hashqueue)

  # ibv thread domain
  option(LCIXX_USE_IBV_TD_DEFAULT
         "Try to lock the IBV queue pair before access it." ON)

  # progress endpoint
  option(LCIXX_USE_PRG_ENDPOINT_DEFAULT
         "Enable the progress specific network endpoint by default." ON)

  # optimize for native
  include(CheckCCompilerFlag)
  check_c_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
  option(LCIXX_OPTIMIZE_FOR_NATIVE "Build with -march=native"
         ${COMPILER_SUPPORTS_MARCH_NATIVE})
  if(LCIXX_OPTIMIZE_FOR_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    else()
      message(
        FATAL_ERROR
          "LCIXX_OPTIMIZE_FOR_NATIVE is set explicitly but the C compiler doesn't support -march=native"
      )
    endif()
  endif()

  # avx
  option(LCIXX_USE_AVX "Use GCC vector extension for the immediate field" OFF)
  if(LCIXX_USE_AVX)
    check_c_compiler_flag("-mavx" COMPILER_SUPPORTS_MAVX)
    if(NOT COMPILER_SUPPORTS_MAVX)
      message(
        FATAL_ERROR
          "COMPILER_SUPPORTS_MAVX is set explicitly but the C compiler doesn't support -mavx"
      )
    endif()
  endif()
  if(LCIXX_USE_AVX)
    target_compile_options(LCIXX PUBLIC -mavx)
  endif()

  # rendezvous protocol
  set(LCIXX_USE_RDV_PROTOCOL_DEFAULT
      writeimm
      CACHE STRING "The default rendezvous protocol to use (write, writeimm).")
  set_property(CACHE LCIXX_USE_RDV_PROTOCOL_DEFAULT PROPERTY STRINGS write
                                                             writeimm)

  # max single message size
  set(LCIXX_USE_MAX_SINGLE_MESSAGE_SIZE_DEFAULT
      0x7FFFFFFF
      CACHE STRING "Default single low-level message max size")

  # papi
  find_package(PAPI)
  option(LCIXX_USE_PAPI "Use PAPI to collect hardware counters" ${PAPI_FOUND})
  if(LCIXX_USE_PAPI)
    if(NOT PAPI_FOUND)
      message(FATAL_ERROR "LCIXX_USE_PAPI is enabled but papi is not found")
    endif()
    target_link_libraries(LCIXX PRIVATE Papi::papi)
  endif()

  # ############################################################################
  mark_as_advanced(
    LCIXX_CONFIG_USE_ALIGNED_ALLOC
    LCIXX_PACKET_SIZE_DEFAULT
    LCIXX_BACKEND_MAX_SENDS_DEFAULT
    LCIXX_BACKEND_MAX_RECVS_DEFAULT
    LCIXX_BACKEND_MAX_CQES_DEFAULT
    LCIXX_PACKET_NUM_DEFAULT
    LCIXX_CACHE_LINE
    LCIXX_USE_RDV_PROTOCOL_DEFAULT
    LCIXX_USE_MAX_SINGLE_MESSAGE_SIZE_DEFAULT)

  # ############################################################################
  # LCIXX Testing related options
  # ############################################################################
  set(LCIXX_USE_CTEST_EXE
      mpirun
      CACHE STRING "exective to be used in ctest")
  set(LCIXX_USE_CTEST_ARGS
      ""
      CACHE STRING "arguments to be used in ctest")

  # ############################################################################
  # Add Subdirectories
  # ############################################################################
  add_subdirectory(src)
  add_subdirectory(third_party)

  if(LCIXX_WITH_EXAMPLES)
    add_subdirectory(examples)
  endif()
  if(LCIXX_WITH_BENCHMARKS)
    add_subdirectory(benchmarks)
  endif()
  if(LCIXX_WITH_TESTS)
    add_subdirectory(tests)
  endif()
  if(LCIXX_WITH_DOC)
    add_subdirectory(doc)
  endif()
endif() # if (NOT LCIXX_WITH_LCT_ONLY)

# ##############################################################################
# Install
# ##############################################################################
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(PKGCONFIG_REQUIRES_PRIVATE ${Fabric_${FABRIC}_PC_Requires})
  set(PKGCONFIG_LIBS_PRIVATE ${Fabric_${FABRIC}_PC_Libs})
  configure_file(liblcixx.pc.in liblcixx.pc @ONLY)

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LCIXXConfigVersion.cmake"
    COMPATIBILITY ExactVersion)
  configure_package_config_file(
    LCIXXConfig.cmake.in LCIXXConfig.cmake
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR)
  install(
    TARGETS LCT
    EXPORT LCIXXTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(
    DIRECTORY lct/api/ ${CMAKE_CURRENT_BINARY_DIR}/lct/api/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h")

  if(NOT LCIXX_WITH_LCT_ONLY)
    install(
      TARGETS LCIXX
      EXPORT LCIXXTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    if(TARGET lcixx-ucx)
      install(
        TARGETS lcixx-ucx
        EXPORT LCIXXTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    install(
      DIRECTORY src/api/ ${CMAKE_CURRENT_BINARY_DIR}/src/api/
                src/binding/generated/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING
      PATTERN "*.hpp")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblcixx.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
    install(
      DIRECTORY cmake_modules/
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI
      FILES_MATCHING
      PATTERN "*.cmake")
    install(PROGRAMS lcrun DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()
  install(
    EXPORT LCIXXTargets
    FILE LCIXXTargets.cmake
    NAMESPACE LCIXX::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCIXX)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LCIXXConfig.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/LCIXXConfigVersion.cmake"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
endif()
