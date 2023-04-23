cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(
    LCI
    VERSION 0.2.1
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
# What parts of LCI to build
# ##############################################################################
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(LCI_WITH_EXAMPLES "Build LCI examples" ON)
option(LCI_WITH_TESTS "Build LCI tests" ON)
option(LCI_WITH_BENCHMARKS "Build LCI benchmarks" ON)
option(LCI_WITH_DOC "Build LCI documentation" ON)

# ##############################################################################
# LCI Optimization Options
# ##############################################################################
option(LCI_DEBUG "LCI Debug Mode" OFF)
option(LCI_USE_MUTEX_CQ
       "Use mutex lock to ensure the thread safety of the completion queue."
       OFF)
option(LCI_SINGLE_THREAD_PROGRESS_DEFAULT
       "LCI_progress will not be called by multiple threads simultaneously" ON)
option(LCI_IBV_ENABLE_TRY_LOCK_QP
       "Try to lock the queue pair before access it." ON)
option(LCI_CONFIG_USE_ALIGNED_ALLOC "Enable memory alignment" ON)
set(LCI_USE_DREG_DEFAULT
    1
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
set(LCI_CACHE_LINE
    64
    CACHE STRING "Size of cache line (bytes)")
option(LCI_USE_PERFORMANCE_COUNTER "Use performance counter" OFF)
option(LCI_ENABLE_SLOWDOWN "Enable manually slowdown" OFF)

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

check_c_compiler_flag("-mavx" COMPILER_SUPPORTS_MAVX)
option(LCI_USE_AVX "Use GCC vector extension for the immediate field"
       ${COMPILER_SUPPORTS_MAVX})
if(LCI_USE_AVX)
  if(NOT COMPILER_SUPPORTS_MAVX)
    message(
      FATAL_ERROR
        "COMPILER_SUPPORTS_MAVX is set explicitly but the C compiler doesn't support -mavx"
    )
  endif()
endif()

mark_as_advanced(
  LCI_CONFIG_USE_ALIGNED_ALLOC
  LCI_PACKET_SIZE_DEFAULT
  LCI_SERVER_MAX_SENDS_DEFAULT
  LCI_SERVER_MAX_RECVS_DEFAULT
  LCI_SERVER_MAX_CQES_DEFAULT
  LCI_SERVER_NUM_PKTS_DEFAULT
  LCI_CACHE_LINE)

# ##############################################################################
# LCI Testing related options
# ##############################################################################
set(SRUN_EXE
    mpirun
    CACHE STRING "exective to be used in ctest")
set(SRUN_EXTRA_ARG
    ""
    CACHE STRING "arguments to be used in ctest")

# ##############################################################################
# LCI Specific Components to build
# ##############################################################################
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

# ##############################################################################
# Figure out which network backend to use
# ##############################################################################
set(LCI_SERVER
    ibv
    CACHE
      STRING
      "Network backend to use. If LCI_FORCE_SERVER is set to OFF (default value),
    this variable is treated as a hint. Otherwise, it is treated as a requirement."
)
set_property(CACHE LCI_SERVER PROPERTY STRINGS ofi ibv)
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
string(TOUPPER ${LCI_SERVER} LCI_SERVER_UPPER)
if(LCI_FORCE_SERVER AND NOT LCI_SERVER_UPPER STREQUAL FABRIC)
  message(
    FATAL_ERROR
      "LCI_FORCE_SERVER is set but the only available backend (${FABRIC})
  is different from what is set in LCI_SERVER (${LCI_SERVER_UPPER}). Give up!")
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
else()
  set(LCI_USE_SERVER_IBV ON)
  message(STATUS "Use ibv(libibverbs) as the network backend")
endif()

# ##############################################################################
# Find More Libraries
# ##############################################################################
find_package(Threads REQUIRED)
find_package(PAPI)
option(LCI_USE_PAPI "Use PAPI to collect hardware counters" ${PAPI_FOUND})
if(LCI_USE_PAPI AND NOT PAPI_FOUND)
  message(FATAL_ERROR "LCI_USE_PAPI is enabled but papi is not found")
endif()

# ##############################################################################
# Add the actual LCI library
# ##############################################################################
add_library(LCI)
set_target_properties(
  LCI
  PROPERTIES C_VISIBILITY_PRESET hidden
             C_STANDARD 11
             C_EXTENSIONS ON)
target_compile_definitions(LCI PRIVATE _GNU_SOURCE)
target_link_libraries(LCI PUBLIC Threads::Threads ${FABRIC}::${FABRIC})
if(LCI_USE_AVX)
  target_compile_options(LCI PUBLIC -mavx)
endif()
if(LCI_USE_PAPI)
  target_link_libraries(LCI PRIVATE Papi::papi)
endif()

# set_target_properties(LCI PROPERTIES OUTPUT_NAME lci)
add_subdirectory(src)
add_subdirectory(dependency)
target_link_libraries(LCI PRIVATE lci-ucx)
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

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LCIConfigVersion.cmake"
    COMPATIBILITY ExactVersion)
  configure_package_config_file(
    LCIConfig.cmake.in LCIConfig.cmake
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR)
  set(PKGCONFIG_REQUIRES_PRIVATE ${Fabric_${FABRIC_PREFER}_PC_Requires})
  set(PKGCONFIG_LIBS_PRIVATE ${Fabric_${FABRIC_PREFER}_PC_Libs})
  configure_file(liblci.pc.in liblci.pc @ONLY)

  install(
    TARGETS LCI
    EXPORT LCITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(
    TARGETS lci-ucx
    EXPORT LCITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(
    EXPORT LCITargets
    FILE LCITargets.cmake
    NAMESPACE LCI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI)
  install(
    DIRECTORY src/api/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h")
  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/api/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblci.pc"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LCIConfig.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/LCIConfigVersion.cmake"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/Find${FABRIC}.cmake"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI)
  install(PROGRAMS lcrun DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
