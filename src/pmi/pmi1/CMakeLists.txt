target_include_directories(pmi-obj PRIVATE include)
target_sources_relative(pmi-obj PRIVATE
        simple/simple_pmi.c
        simple/simple_pmiutil.c
)

add_executable(pmi-test simple/simple_pmi.c simple/simple_pmiutil.c simple/test.c)
target_include_directories(pmi-test PRIVATE include)
target_compile_definitions(pmi-test PRIVATE _GNU_SOURCE)
set_target_properties(pmi-test PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
