cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(
    LCI
    VERSION 1.7.6
    DESCRIPTION "Lightweight Communication Interface"
    HOMEPAGE_URL "https://github.com/uiuc-hpc/LC")
  enable_testing()
endif()

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
target_link_libraries(LCT PUBLIC Threads::Threads)
add_subdirectory(lct)

if(NOT LCI_WITH_LCT_ONLY)
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
  set(LCI_SERVER_DEFAULT ibv)
  # If user has explicitly defined LCI_SERVER, we can skip the tests for the
  # default option
  if(NOT DEFINED LCI_SERVER)
    # If we find the libfabric/cxi provider, set the default backend to OFI
    execute_process(COMMAND bash "-c" "fi_info -l | grep cxi -q"
                    RESULT_VARIABLE OFI_CXI_NOT_FOUND)
    if(NOT OFI_CXI_NOT_FOUND)
      message(
        STATUS
          "Found the libfabric/cxi provider, set the default backend to OFI")
      set(LCI_SERVER_DEFAULT ofi)
    endif()
  endif()

  set(LCI_SERVER
      ${LCI_SERVER_DEFAULT}
      CACHE
        STRING
        "Network backend to use. If LCI_FORCE_SERVER is set to OFF (default value),
      this variable is treated as a hint. Otherwise, it is treated as a requirement."
  )
  set_property(CACHE LCI_SERVER PROPERTY STRINGS ofi ibv ucx)
  option(LCI_FORCE_SERVER
         "Force LCI to use the network backend specified by LCI_SERVER" OFF)
  set(LCI_OFI_PROVIDER_HINT_DEFAULT
      ""
      CACHE
        STRING
        "If using the ofi(libfabric) backend, provide a hint for the provider to use"
  )

  find_package(OFI)
  find_package(IBV)
  find_package(ucx)
  if(IBV_FOUND AND OFI_FOUND)
    if(LCI_SERVER STREQUAL "ofi")
      set(FABRIC OFI)
    else()
      set(FABRIC IBV)
    endif()
  elseif(IBV_FOUND)
    set(FABRIC IBV)
  elseif(OFI_FOUND)
    set(FABRIC OFI)
  else()
    message(FATAL_ERROR "Find neither libfabric nor libibverbs. Give up!")
  endif()
  if(LCI_SERVER STREQUAL "ucx")
    if(NOT ucx_FOUND)
      message(FATAL_ERROR "ucx is chosen as network backend but not found!")
    endif()
    set(FABRIC ucx)
  endif()
  string(TOUPPER ${LCI_SERVER} LCI_SERVER_UPPER)
  if(LCI_FORCE_SERVER AND NOT LCI_SERVER_UPPER STREQUAL FABRIC)
    message(
      FATAL_ERROR
        "LCI_FORCE_SERVER is set but the only available backend (${FABRIC})
    is different from what is set in LCI_SERVER (${LCI_SERVER_UPPER}). Give up!"
    )
  endif()
  set(LCI_FABRIC
      ${FABRIC}
      CACHE
        STRING
        "Used as an output variable if LCI is included in a larger project via FetchContent"
        FORCE)

  if(FABRIC STREQUAL OFI)
    set(LCI_USE_SERVER_OFI ON)
    message(STATUS "Use ofi(libfabric) as the network backend")
  elseif(FABRIC STREQUAL IBV)
    set(LCI_USE_SERVER_IBV ON)
    message(STATUS "Use ibv(libibverbs) as the network backend")
  else()
    set(LCI_USE_SERVER_UCX ON)
    message(STATUS "Use ucx as the network backend")
  endif()

  # ############################################################################
  # LCI Optimization Options
  # ############################################################################
  option(LCI_DEBUG "LCI Debug Mode" OFF)
  option(
    LCI_USE_INLINE_CQ
    "Use the C version of the completion queue so that it could be compiled inline."
    OFF)
  option(LCI_ENABLE_MULTITHREAD_PROGRESS
         "LCI_progress can be called by multiple threads simultaneously" ON)
  option(LCI_CONFIG_USE_ALIGNED_ALLOC "Enable memory alignment" ON)
  set(LCI_USE_DREG_DEFAULT
      ${LCI_USE_SERVER_IBV}
      CACHE STRING "Whether to use registration cache")
  set(LCI_PACKET_SIZE_DEFAULT
      12288
      CACHE STRING "Size of packet")
  set(LCI_SERVER_MAX_SENDS_DEFAULT
      64
      CACHE STRING "Max posted sends")
  set(LCI_SERVER_MAX_RECVS_DEFAULT
      1024
      CACHE STRING "Max posted recvs")
  set(LCI_SERVER_MAX_CQES_DEFAULT
      65536
      CACHE STRING "Max posted cqes")
  set(LCI_SERVER_NUM_PKTS_DEFAULT
      8192
      CACHE STRING "Number of packets")
  set(LCI_MT_BACKEND_DEFAULT
      "hashqueue"
      CACHE STRING "The default matching table backend to use.")
  set_property(CACHE LCI_MT_BACKEND_DEFAULT PROPERTY STRINGS hash queue
                                                     hashqueue)
  set(LCI_CACHE_LINE
      64
      CACHE STRING "Size of cache line (bytes)")
  option(LCI_USE_PERFORMANCE_COUNTER "Use performance counter" OFF)
  option(LCI_ENABLE_SLOWDOWN "Enable manually slowdown" OFF)
  option(LCI_IBV_ENABLE_TD_DEFAULT
         "Try to lock the IBV queue pair before access it." ON)
  option(LCI_ENABLE_PRG_NET_ENDPOINT_DEFAULT
         "Enable the progress specific network endpoint by default." ON)

  option(LCI_USE_GPROF "Enable GPROF" OFF)
  if(LCI_USE_GPROF)
    add_compile_options(-pg -fno-inline)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")
  endif()

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

  set(LCI_RDV_PROTOCOL_DEFAULT
      writeimm
      CACHE STRING "The default rendezvous protocol to use (write, writeimm).")
  set_property(CACHE LCI_RDV_PROTOCOL_DEFAULT PROPERTY STRINGS write writeimm)

  mark_as_advanced(
    LCI_CONFIG_USE_ALIGNED_ALLOC
    LCI_PACKET_SIZE_DEFAULT
    LCI_SERVER_MAX_SENDS_DEFAULT
    LCI_SERVER_MAX_RECVS_DEFAULT
    LCI_SERVER_MAX_CQES_DEFAULT
    LCI_SERVER_NUM_PKTS_DEFAULT
    LCI_CACHE_LINE
    LCI_RDV_PROTOCOL_DEFAULT)

  # ############################################################################
  # LCI Testing related options
  # ############################################################################
  set(SRUN_EXE
      mpirun
      CACHE STRING "exective to be used in ctest")
  set(SRUN_EXTRA_ARG
      ""
      CACHE STRING "arguments to be used in ctest")

  # ############################################################################
  # LCI Specific Components to build
  # ############################################################################
  set(LCI_EP_CE
      sync cq am
      CACHE STRING "Completion mechanism (sync, cq, am, glob)")
  if("sync" IN_LIST LCI_EP_CE)
    set(LCI_SERVER_HAS_SYNC ON)
  endif()
  if("cq" IN_LIST LCI_EP_CE)
    set(LCI_SERVER_HAS_CQ ON)
  endif()
  if("am" IN_LIST LCI_EP_CE)
    set(LCI_SERVER_HAS_AM ON)
  endif()
  if("glob" IN_LIST LCI_EP_CE)
    set(LCI_SERVER_HAS_GLOB ON)
  endif()

  # ############################################################################
  # Find More Libraries
  # ############################################################################
  find_package(PAPI)
  option(LCI_USE_PAPI "Use PAPI to collect hardware counters" ${PAPI_FOUND})
  if(LCI_USE_PAPI AND NOT PAPI_FOUND)
    message(FATAL_ERROR "LCI_USE_PAPI is enabled but papi is not found")
  endif()

  # ############################################################################
  # Add the actual LCI library
  # ############################################################################

  add_library(LCI)
  set_target_properties(
    LCI
    PROPERTIES C_VISIBILITY_PRESET hidden
               C_STANDARD 11
               C_EXTENSIONS ON)
  target_compile_definitions(LCI PRIVATE _GNU_SOURCE)
  if(FABRIC STREQUAL ucx)
    target_link_libraries(LCI PUBLIC Threads::Threads ${FABRIC}::ucp LCT)
  else()
    target_link_libraries(LCI PUBLIC Threads::Threads ${FABRIC}::${FABRIC} LCT)
  endif()
  if(LCI_USE_AVX)
    target_compile_options(LCI PUBLIC -mavx)
  endif()
  if(LCI_USE_PAPI)
    target_link_libraries(LCI PRIVATE Papi::papi)
  endif()

  set_target_properties(LCI PROPERTIES OUTPUT_NAME lci)
  add_subdirectory(lci)
  add_subdirectory(dependency)
  target_link_libraries(LCI PRIVATE lci-ucx)

  # ############################################################################
  # Build other targets
  # ############################################################################
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
      TARGETS LCI lci-ucx
      EXPORT LCITargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(
      DIRECTORY lci/api/ ${CMAKE_CURRENT_BINARY_DIR}/lci/api/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING
      PATTERN "*.h")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblci.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
    install(
      FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/Find${FABRIC}.cmake"
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI)
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
