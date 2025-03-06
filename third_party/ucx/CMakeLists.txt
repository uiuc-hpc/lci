include(CheckSymbolExists)
include(CheckStructMember)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckTypeSize)

file(GLOB_RECURSE lci-ucx_SRC CONFIGURE_DEPENDS "*.h" "*.c")

add_library(lci-ucx SHARED ${lci-ucx_SRC} lci_ucx_api.c)
set_target_properties(lci-ucx PROPERTIES C_VISIBILITY_PRESET hidden)
find_package(Threads REQUIRED)
target_link_libraries(lci-ucx PRIVATE Threads::Threads)
target_link_libraries(lci-ucx PRIVATE rt)
target_link_libraries(lci-ucx PRIVATE m)
target_include_directories(lci-ucx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                           ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(lci-ucx PRIVATE _GNU_SOURCE=1)
target_compile_definitions(lci-ucx PRIVATE HAVE_CONFIG_H)
target_compile_definitions(lci-ucx PRIVATE UCX_SHARED_LIBRARY)
target_compile_definitions(lci-ucx PRIVATE "UCM_MALLOC_PREFIX=ucm_dl")
target_compile_definitions(lci-ucx PRIVATE "CPU_FLAGS=")
target_compile_options(lci-ucx PRIVATE "-Wno-deprecated-declarations")

include(GNUInstallDirs)
set(UCX_MODULE_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lci-ucx)
set(UCX_CONFIG_DIR ${SYSCONFDIR})

# configure.ac

set(LCI_UCX_ENABLE_DEBUG_DATA
    OFF
    CACHE STRING "Enable collecting data to ease debugging, default: NO")
if(LCI_UCX_ENABLE_DEBUG_DATA)
  set(ENABLE_DEBUG_DATA ON)
endif()

# config/m4/sysdep.m4

set(LCI_UCX_ENABLE_VALGRIND
    OFF
    CACHE STRING
          "Enable Valgrind annotations (small runtime overhead, default NO)")
if(NOT LCI_UCX_ENABLE_VALGRIND)
  set(NVALGRIND ON)
endif()

check_struct_member("struct sigevent" "_sigev_un._tid" "signal.h"
                    HAVE_SIGEVENT_SIGEV_UN_TID)
check_struct_member("struct sigevent" "sigev_notify_thread_id" "signal.h"
                    HAVE_SIGEVENT_SIGEV_NOTIFY_THREAD_ID)

include(CheckIncludeFiles)
check_include_files(malloc.h HAVE_MALLOC_H)
check_include_files(malloc_np.h HAVE_MALLOC_NP_H)

set(CMAKE_REQUIRED_FLAGS "-Wno-deprecated-declarations")
check_c_source_runs(
  "
        #include <malloc.h>
        static int rc = 1;
        void *ptr;
        void *myhook(size_t size, const void *caller) {
          rc = 0;
          return NULL;
        }
        int main(int argc, char** argv) {
          __malloc_hook = myhook;
          ptr = malloc(1);
          return rc;
        }
        "
  CHECK_MALLOC_HOOK_RET_VAL)
set(CMAKE_REQUIRED_FLAGS)
if(CHECK_MALLOC_HOOK_RET_VAL)
  set(HAVE_MALLOC_HOOK ON)
else()
  message(WARNING "malloc hooks are not supported.")
endif()

# Misc. Linux-specific functions
set(CMAKE_EXTRA_INCLUDE_FILES sched.h)
set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")
check_function_exists(clearenv HAVE_CLEARENV)
check_function_exists(malloc_trim HAVE_MALLOC_TRIM)
check_function_exists(memalign HAVE_MEMALIGN)
check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
check_function_exists(mremap HAVE_MREMAP)
check_function_exists(sched_setaffinity HAVE_SCHED_SETAFFINITY)
check_function_exists(sched_getaffinity HAVE_SCHED_GETAFFINITY)
check_function_exists(cpuset_setaffinity HAVE_CPUSET_SETAFFINITY)
check_function_exists(cpuset_getaffinity HAVE_CPUSET_GETAFFINITY)
set(CMAKE_REQUIRED_DEFINITIONS)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES signal.h)
set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")
check_type_size("sighandler_t" HAVE_SIGHANDLER_T)
check_type_size("__sighandler_t" HAVE___SIGHANDLER_T)
set(CMAKE_REQUIRED_DEFINITIONS)
set(CMAKE_EXTRA_INCLUDE_FILES)

# src/ucs/configure.m4

set(LCI_UCX_ENABLE_PROFILING
    OFF
    CACHE STRING "Enable profiling support")
if(LCI_UCX_ENABLE_PROFILING)
  set(ENABLE_PROFILEING ON)
endif()

set(LCI_UCX_ENABLE_STATS
    OFF
    CACHE STRING "enabling statistics")
if(LCI_UCX_ENABLE_STATS)
  set(ENABLE_STATS ON)
endif()

set(LCI_UCX_ENABLE_TUNING
    OFF
    CACHE STRING "Enable tuning")
if(LCI_UCX_ENABLE_TUNING)
  set(ENABLE_TUNING ON)
endif()

set(LCI_UCX_ENABLE_PROFILING
    OFF
    CACHE STRING "Enable profiling")
if(LCI_UCX_ENABLE_PROFILING)
  set(HAVE_PROFILING ON)
endif()

set(LCI_UCX_MAX_LOG_LEVEL
    debug
    CACHE STRING "Highest log level")
set_property(
  CACHE LCI_UCX_MAX_LOG_LEVEL
  PROPERTY STRINGS
           warn
           diag
           info
           debug
           trace
           trace_req
           trace_data
           trace_async
           trace_func
           trace_poll)
set(UCS_MAX_LOG_LEVEL UCS_LOG_LEVEL_${LCI_UCX_MAX_LOG_LEVEL})
string(TOUPPER ${UCS_MAX_LOG_LEVEL} UCS_MAX_LOG_LEVEL)

set(LCI_UCX_ENABLE_ASSERT
    OFF
    CACHE STRING "Enable assertions")
if(LCI_UCX_ENABLE_ASSERT OR LCI_DEBUG)
  set(ENABLE_ASSERT ON)
endif()

# Check if __attribute__((constructor)) works
check_c_source_runs(
  "
        static int rc = 1;
        static void constructor_test() __attribute__((constructor));
        static void constructor_test() { rc = 0; }
        int main(int argc, char** argv) { return rc; }
        "
  CHECK_ATTR_RET_VAL)
if(NOT CHECK_ATTR_RET_VAL)
  message(
    FATAL_ERROR
      "Cannot continue. Please use compiler that supports __attribute__((constructor))."
  )
endif()

set(LCI_UCX_HAVE_CACHE_LINE_SIZE
    OFF
    CACHE STRING "user defined cache line size")
set_property(CACHE LCI_UCX_HAVE_CACHE_LINE_SIZE PROPERTY STRINGS OFF 64 128)
if(LCI_UCX_HAVE_CACHE_LINE_SIZE)
  set(HAVE_CACHE_LINE_SIZE ${LCI_UCX_HAVE_CACHE_LINE_SIZE})
endif()

# Check the existence of hardware timer
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  check_c_source_runs(
    "#include <stdint.h>
        int main(int argc, char** argv) { uint64_t tmp; asm volatile(\"mrs %0, cntvct_el0\" : \"=r\" (tmp)); }
        "
    CHECK_TIMER_RET_VAL)
  if(CHECK_TIMER_RET_VAL)
    set(HAVE_HW_TIMER ON)
    message(STATUS "high-resolution hardware timer enabled")
  else()
    message(STATUS "high-resolution hardware timer disabled")
  endif()
else()
  # HW timer is supported for all other architectures
  set(HAVE_HW_TIMER ON)
  message(STATUS "high-resolution hardware timer enabled")
endif()

set(LCI_UCX_ENABLE_BUILTIN_MEMCPY
    ON
    CACHE STRING "Enable builtin memcpy")
if(LCI_UCX_ENABLE_BUILTIN_MEMCPY)
  set(ENABLE_BUILTIN_MEMCPY ON)
endif()

check_function_exists(__clear_cache HAVE___CLEAR_CACHE)
check_function_exists(__aarch64_sync_cache_range
                      HAVE___AARCH64_SYNC_CACHE_RANGE)

# config/m4/ucm.m4

# Memory allocator selection
set(LCI_UCX_WITH_ALLOCATOR
    ptmalloc286
    CACHE STRING
          "Build UCX with predefined memory allocator. The supported values are:
         ptmalloc286. Default: ptmalloc286")
set_property(CACHE LCI_UCX_WITH_ALLOCATOR PROPERTY STRINGS ptmalloc286)
if(LCI_UCX_WITH_ALLOCATOR STREQUAL "ptmalloc286")
  set(HAVE_UCM_PTMALLOC286 ON)
endif()

if(HAVE_UCM_PTMALLOC286)
  target_compile_options(lci-ucx PRIVATE "-fno-strict-aliasing")
  target_compile_options(lci-ucx PRIVATE "-DUSE_LOCKS=1")
  target_compile_options(lci-ucx PRIVATE "-DMALLINFO_FIELD_TYPE=int")
endif()

# Madvise flags
check_symbol_exists(MADV_FREE "sys/mman.h" HAVE_DECL_MADV_FREE)
check_symbol_exists(MADV_REMOVE "sys/mman.h" HAVE_DECL_MADV_REMOVE)
check_symbol_exists(POSIX_MADV_DONTNEED "sys/mman.h"
                    HAVE_DECL_POSIX_MADV_DONTNEED)

# getauxval()
check_symbol_exists(getauxval "sys/auxv.h" HAVE_DECL_GETAUXVAL)

# BISTRO hooks infrastructure
#
# SYS_xxx macro
#
check_symbol_exists(SYS_mmap "sys/syscall.h" HAVE_DECL_SYS_MMAP)
check_symbol_exists(SYS_munmap "sys/syscall.h" HAVE_DECL_SYS_MUNMAP)
check_symbol_exists(SYS_mremap "sys/syscall.h" HAVE_DECL_SYS_MREMAP)
check_symbol_exists(SYS_brk "sys/syscall.h" HAVE_DECL_SYS_BRK)
check_symbol_exists(SYS_madvise "sys/syscall.h" HAVE_DECL_SYS_MADVISE)

if(HAVE_DECL_SYS_MMAP
   AND HAVE_DECL_SYS_MUNMAP
   AND HAVE_DECL_SYS_MREMAP
   AND HAVE_DECL_SYS_BRK
   AND HAVE_DECL_SYS_MADVISE)
  set(MMAP_HOOKS_HAPPY ON)
endif()

check_symbol_exists(SYS_shmat "sys/syscall.h" HAVE_DECL_SYS_SHMAT)
check_symbol_exists(SYS_shmdt "sys/syscall.h" HAVE_DECL_SYS_SHMDT)

if(HAVE_DECL_SYS_SHMAT AND HAVE_DECL_SYS_SHMDT)
  set(SHM_HOOKS_HAPPY ON)
endif()

check_symbol_exists(SYS_ipc "sys/syscall.h" HAVE_DECL_SYS_IPC)
if(HAVE_DECL_SYS_IPC)
  set(IPC_HOOKS_HAPPY ON)
endif()

if(MMAP_HOOKS_HAPPY AND (SHM_HOOKS_HAPPY OR IPC_HOOKS_HAPPY))
  set(BISTRO_HOOKS_HAPPY ON)
endif()

if(BISTRO_HOOKS_HAPPY)
  set(UCM_BISTRO_HOOKS ON)
else()
  message(WARN "Some of required syscalls could not be found\n"
          "BISTRO mmap hook mode is disabled")
endif()

configure_file(config.h.in config.h @ONLY)
