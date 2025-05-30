if(NOT LCI_USE_CUDA)
  return()
endif()

target_sources_relative(LCI PRIVATE accelerator_cuda.cpp)
