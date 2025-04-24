target_include_directories(${LIBRARY_NAME} PRIVATE include)
target_sources_relative(${LIBRARY_NAME} PRIVATE client/pmi2_util.c
                        client/pmi2_api.c client/pmi2_util.h)

add_executable(pmi2-test client/test.c client/pmi2_util.c client/pmi2_api.c
                         client/pmi2_util.h)
target_include_directories(pmi2-test PRIVATE include)
set_target_properties(pmi2-test PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
