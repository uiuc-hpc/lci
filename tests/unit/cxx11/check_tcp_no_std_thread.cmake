# Copyright (c) 2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

if(NOT DEFINED BACKEND_SOURCE)
  message(FATAL_ERROR "BACKEND_SOURCE is required")
endif()
file(READ "${BACKEND_SOURCE}" backend_source)
string(FIND "${backend_source}" "std::thread" std_thread_pos)
if(NOT std_thread_pos EQUAL -1)
  message(
    FATAL_ERROR
      "torchrun TCP PMI backend must not create std::thread background workers")
endif()
string(FIND "${backend_source}" "#include <thread>" include_thread_pos)
if(NOT include_thread_pos EQUAL -1)
  message(FATAL_ERROR "torchrun TCP PMI backend must not include <thread>")
endif()
