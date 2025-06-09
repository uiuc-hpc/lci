function(add_lci_bench_resources)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name})
    target_link_libraries(${name_without_ext} PRIVATE LCT Threads::Threads)
    target_include_directories(
      ${name_without_ext}
      PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src>)
  endforeach()
  add_lci_tests(TESTS ${ARGN} LABELS benchmark COMMANDS
                "${LCI_USE_CTEST_ARGS} [TARGET]")
endfunction()

add_lci_bench_resources(
  bench_packet_pool.cpp
  bench_matching_engine.cpp
  bench_cq.cpp
  bench_malloc.cpp
  bench_memcpy.cpp
  bench_atomic.cpp
  bench_mem_reg.cpp)

if(NOT LCI_USE_CUDA)
  return()
endif()

function(add_lci_bench_resources_cuda)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name})
    target_link_libraries(${name_without_ext} PRIVATE LCT Threads::Threads
                                                      CUDA::cudart)
    target_include_directories(
      ${name_without_ext}
      PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src>)
    set_target_cuda_standard(${name_without_ext} STANDARD ${LCI_CUDA_STANDARD})
    set_target_cuda_architectures(${name_without_ext} ARCHITECTURES
                                  ${LCI_CUDA_ARCH})
    set_target_cuda_warnings_and_errors(${name_without_ext} WARN_AS_ERROR
                                        ${LCI_BUILD_WARN_AS_ERROR})
  endforeach()
  add_lci_tests(
    TESTS
    ${ARGN}
    LABELS
    benchmark
    COMMANDS
    "${LCI_USE_CTEST_ARGS} [TARGET]"
    DEPENDENCIES
    CUDA::cudart)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    set_target_cuda_standard(test-benchmark-${name_without_ext} STANDARD
                             ${LCI_CUDA_STANDARD})
    set_target_cuda_architectures(test-benchmark-${name_without_ext}
                                  ARCHITECTURES ${LCI_CUDA_ARCH})
    set_target_cuda_warnings_and_errors(
      test-benchmark-${name_without_ext} WARN_AS_ERROR
      ${LCI_BUILD_WARN_AS_ERROR})
  endforeach()
endfunction()

add_lci_bench_resources_cuda(bench_cuda_memcpy.cu bench_get_buffer_attr.cpp)

target_include_directories(bench_get_buffer_attr
                           PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(test-benchmark-bench_get_buffer_attr
                           PRIVATE ${PROJECT_SOURCE_DIR}/src)
get_target_property(LCI_LINKED_LIBS LCI LINK_LIBRARIES)
target_link_libraries(bench_get_buffer_attr PRIVATE ${LCI_LINKED_LIBS})
target_link_libraries(test-benchmark-bench_get_buffer_attr
                      PRIVATE ${LCI_LINKED_LIBS})
