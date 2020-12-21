target_include_directories(lci-obj PRIVATE include)

add_executable(pmi2-test test.c)
target_include_directories(pmi2-test PRIVATE include)
target_compile_definitions(pmi2-test PRIVATE _GNU_SOURCE)
set_target_properties(pmi2-test PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
target_link_libraries(pmi2-test PRIVATE PMI::PMI2)
