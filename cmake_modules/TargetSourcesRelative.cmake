function(target_sources_relative target)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
    cmake_policy(PUSH)
    cmake_policy(SET CMP0076 NEW)
    target_sources(${target} ${ARGN})
    cmake_policy(POP)
  else()
    set(keywords INTERFACE PUBLIC PRIVATE)
    cmake_parse_arguments(target_sources "" "" "${keywords}" ${ARGN})
    foreach(kw ${keywords})
        list(TRANSFORM target_sources_${kw}
          PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
          OUTPUT_VARIABLE ${kw}_RELATIVE
        )
    endforeach()
    target_sources(${target}
      INTERFACE ${INTERFACE_RELATIVE}
      PUBLIC ${PUBLIC_RELATIVE}
      PRIVATE ${PRIVATE_RELATIVE}
    )
  endif()
endfunction()