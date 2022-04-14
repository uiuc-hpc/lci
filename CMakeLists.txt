cmake_minimum_required(VERSION 3.12)
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
project(LCI
        VERSION 0.2.1
        DESCRIPTION "Lightweight Communication Interface"
        HOMEPAGE_URL "https://github.com/uiuc-hpc/LC"
        )
enable_testing()
endif()

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(LCI_DEBUG "LCI Debug Mode" OFF)
option(LCI_USE_DREG "Use registration cache (can be buggy when the allocation/free is too dynamic)" ON)
option(LCI_CONFIG_USE_ALIGNED_ALLOC "Enable memory alignment" ON)
option(LCI_WITH_EXAMPLES "Build LCI examples" ON)
option(LCI_WITH_TESTS "Build LCI tests" ON)
option(LCI_WITH_BENCHMARKS "Build LCI benchmarks" ON)
option(LCI_WITH_DOC "Build LCI documentation" ON)
set(LCI_SERVER ofi CACHE STRING "Fabric")
set_property(CACHE LCI_SERVER PROPERTY STRINGS ofi ibv psm2)
set(LCI_EP_AR dynamic explicit CACHE STRING "Addressing mode (dynamic, explicit, immediate)")
set(LCI_EP_CE sync cq am CACHE STRING "Completion mechanism (sync, cq, am, glob)")
set(LCI_MAX_DEV 1 CACHE STRING "Maximum number of devices")
set(LCI_MAX_EP 8 CACHE STRING "Maximum number of EP")
set(LCI_SYNC flag CACHE STRING "Sync mechanism")
set_property(CACHE LCI_SYNC PROPERTY STRINGS flag argobots)

set(LCI_PACKET_SIZE ${LCI_PACKET_SIZE} CACHE STRING "Size of packet")
set(LCI_MAX_INLINE ${LCI_MAX_INLINE} CACHE STRING "Max inline message size")
set(LCI_DEV_MEM_SIZE "(64*1024*1024)" CACHE STRING "Size of device memory")
set(LCI_SERVER_MAX_RCVS 64 CACHE STRING "Max posted recvs")
set(LCI_SERVER_MAX_CQES 65536 CACHE STRING "Max posted cqes")
set(LCI_SERVER_NUM_PKTS 1024 CACHE STRING "Number of packets")
set(LCI_CACHE_LINE 64 CACHE STRING "Size of cache line (bytes)")
option(LCI_USE_AVX "Use GCC vector extension for the immediate field" ON)
mark_as_advanced(
        LCI_PACKET_SIZE
        LCI_MAX_INLINE
        LCI_DEV_MEM_SIZE
        LCI_SERVER_MAX_RCVS
        LCI_SERVER_NUM_PKTS
        LCI_CACHE_LINE
)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(TargetSourcesRelative)
include(AddLCI)

if(LCI_SERVER STREQUAL "ofi")
  set(LCI_USE_SERVER_OFI ON)
elseif(LCI_SERVER STREQUAL "ibv")
  set(LCI_USE_SERVER_IBV ON)
elseif(LCI_SERVER STREQUAL "psm2")
  set(LCI_USE_SERVER_PSM ON)
else()
  message(FATAL_ERROR "Fabric ${LCI_SERVER} not supported")
endif()

if("dynamic" IN_LIST LCI_EP_AR)
  set(LCI_SERVER_HAS_DYN ON)
endif()
if("explicit" IN_LIST LCI_EP_AR)
  set(LCI_SERVER_HAS_EXP ON)
endif()
if("immediate" IN_LIST LCI_EP_AR)
  set(LCI_SERVER_HAS_IMM ON)
endif()

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

if(LCI_SYNC STREQUAL "flag")
  set(LCI_SYNC_FLAG ON)
elseif(LCI_SYNC STREQUAL "argobots")
  set(LCI_SYNC_ABT ON)
else()
  message(FATAL_ERROR "Sync mechanism ${LCI_SYNC} not supported")
endif()

set(USE_AFFI ON CACHE BOOL "CPU affinity")
mark_as_advanced(USE_AFFI)
if(USE_AFFI)
  set(AFF_DEBUG ON)
  add_compile_definitions(USE_AFFI AFF_DEBUG)
endif()

#NOTE(danghvu): These numbers are tweaked for performance and some alignment.
#Update at our own risk.
if(LCI_USE_SERVER_OFI)
  set(LCI_PACKET_SIZE "(32 * 1024 + 4096)")
else()
  set(LCI_PACKET_SIZE "(8 * 1024 + 4096)")
endif()

if(LCI_USE_SERVER_PSM)
  set(LCI_MAX_INLINE "1024")
else()
  set(LCI_MAX_INLINE "64")
endif()

# find package
#set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
#set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

string(TOUPPER ${LCI_SERVER} FABRIC)
find_package(${FABRIC} REQUIRED)

find_package(Argobots)

option(LCI_USE_GPROF "Enable GPROF" OFF)
if(LCI_USE_GPROF)
  add_compile_options(-pg -fno-inline)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")
endif()

add_library(LCI)
set_target_properties(LCI PROPERTIES
        C_VISIBILITY_PRESET       hidden
        C_STANDARD                11
        C_EXTENSIONS              ON
        )
target_compile_definitions(LCI PRIVATE _GNU_SOURCE)
target_include_directories(LCI PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )
target_link_libraries(LCI PUBLIC
        Threads::Threads
        ${FABRIC}::${FABRIC}
        $<$<BOOL:${LCI_SYNC_ABT}>:Argobots::Argobots>
        )
if(LCI_USE_AVX)
  target_compile_options(LCI PUBLIC -mavx)
endif()

#set_target_properties(LCI PROPERTIES OUTPUT_NAME lci)
add_subdirectory(include)
add_subdirectory(src)
if(LCI_WITH_EXAMPLES)
  #add_subdirectory(examples)
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

if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
  write_basic_package_version_file(
          "${CMAKE_CURRENT_BINARY_DIR}/LCIConfigVersion.cmake"
          COMPATIBILITY ExactVersion
  )
  configure_package_config_file(LCIConfig.cmake.in LCIConfig.cmake
          INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake"
          PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
          )
  set(PKGCONFIG_REQUIRES_PRIVATE ${Fabric_${FABRIC_PREFER}_PC_Requires})
  set(PKGCONFIG_LIBS_PRIVATE ${Fabric_${FABRIC_PREFER}_PC_Libs})
  configure_file(liblci.pc.in liblci.pc @ONLY)

  install(TARGETS LCI
          EXPORT LCITargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          )
  install(EXPORT LCITargets
          FILE LCITargets.cmake
          NAMESPACE LCI::
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI
          )
  install(DIRECTORY include/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.h"
          )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblci.pc"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
          )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LCIConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/LCIConfigVersion.cmake"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake
          )
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindFabric.cmake"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCI
          )
  install(PROGRAMS lcrun DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()