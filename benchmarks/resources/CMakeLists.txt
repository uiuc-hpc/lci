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

add_lci_bench_resources(bench_packet_pool.cpp bench_matching_engine.cpp
                        bench_cq.cpp)
