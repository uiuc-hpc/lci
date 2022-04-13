configure_file(lcii_config.h.in lcii_config.h @ONLY)
target_include_directories(LCI PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
