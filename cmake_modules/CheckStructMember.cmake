# * Check if the given struct or class has the specified member variable
#   CHECK_STRUCT_MEMBER (STRUCT MEMBER HEADER VARIABLE)
#
# STRUCT - the name of the struct or class you are interested in MEMBER - the
# member which existence you want to check HEADER - the header(s) where the
# prototype should be declared VARIABLE - variable to store the result
#
# The following variables may be set before calling this macro to modify the way
# the check is run:
#
# CMAKE_REQUIRED_FLAGS = string of compile command line flags
# CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
# CMAKE_REQUIRED_INCLUDES = list of include directories

# Copyright (c) 2022, Jiakun Yan,         <jiakuny3@illinois.edu> Copyright (c)
# 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.

include(CheckCSourceCompiles)

macro(CHECK_STRUCT_MEMBER _STRUCT _MEMBER _HEADER _RESULT)
  message(STATUS "Looking for ${_STRUCT}.${_MEMBER}")
  set(_INCLUDE_FILES)
  foreach(it ${_HEADER})
    set(_INCLUDE_FILES "${_INCLUDE_FILES}#include <${it}>\n")
  endforeach(it)

  set(_CHECK_STRUCT_MEMBER_SOURCE_CODE
      "
${_INCLUDE_FILES}
int main()
{
   static ${_STRUCT} tmp;
   if (sizeof(tmp.${_MEMBER}))
     return 0;
   else
     return 1;
}
")
  set(CMAKE_REQUIRED_QUIET ON)
  check_c_source_compiles("${_CHECK_STRUCT_MEMBER_SOURCE_CODE}" ${_RESULT})
  set(CMAKE_REQUIRED_QUIET)
  if(${_RESULT})
    message(STATUS "Looking for ${_STRUCT}.${_MEMBER} - found")
  else()
    message(STATUS "Looking for ${_STRUCT}.${_MEMBER} - not found")
  endif()
endmacro(CHECK_STRUCT_MEMBER)
