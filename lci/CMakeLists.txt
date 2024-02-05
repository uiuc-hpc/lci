add_subdirectory(experimental)

target_include_directories(LCI PRIVATE .)
target_sources_relative(
  LCI
  PRIVATE
  log/logger.c
  profile/papi_wrapper.c
  profile/performance_counter.c
  runtime/1sided_primitive.c
  runtime/completion/cq.c
  runtime/lci.c
  runtime/progress.c
  runtime/memory_registration.c
  runtime/matchtable/matchtable.c
  runtime/matchtable/matchtable_hash.c
  runtime/matchtable/matchtable_queue.c
  runtime/matchtable/matchtable_hashqueue.c
  runtime/property_list.c
  runtime/packet_pool.c
  runtime/2sided_primitive.c
  runtime/env.c
  runtime/device.c
  runtime/endpoint.c
  runtime/completion/amhandler.c
  runtime/rcache/lcii_rcache.c
  runtime/completion/sync_flag.c)

if(LCI_USE_SERVER_OFI)
  target_sources_relative(LCI PRIVATE backend/ofi/server_ofi.c)
elseif(LCI_USE_SERVER_IBV)
  target_sources_relative(LCI PRIVATE backend/ibv/server_ibv.c)
  target_sources_relative(LCI PRIVATE backend/ibv/lcisi_ibv_detail.c)
elseif(LCI_USE_SERVER_UCX)
  target_sources_relative(LCI PRIVATE backend/ucx/server_ucx.c)
endif()

target_include_directories(LCI PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(api)
