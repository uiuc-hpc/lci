add_subdirectory(api)
add_subdirectory(pmi)
add_subdirectory(experimental)

target_include_directories(LCI PRIVATE .)
target_sources_relative(
  LCI
  PRIVATE
  log/lcm_log.c
  profile/papi_wrapper.c
  profile/performance_counter.c
  runtime/1sided_primitive.c
  runtime/completion/cq.c
  runtime/lci.c
  runtime/progress.c
  runtime/memory_registration.c
  runtime/matching_table.c
  runtime/property_list.c
  runtime/packet_pool.c
  runtime/2sided_primitive.c
  runtime/env.c
  runtime/device.c
  runtime/endpoint.c
  runtime/completion/amhandler.c
  runtime/monitor_thread.c
  runtime/rcache/lcii_rcache.c
  runtime/completion/sync_flag.c)

if(LCI_USE_SERVER_OFI)
  target_sources_relative(LCI PRIVATE backend/server_ofi.c)
elseif(LCI_USE_SERVER_IBV)
  target_sources_relative(LCI PRIVATE backend/server_ibv.c)
endif()

configure_file(lcii_config.h.in lcii_config.h @ONLY)
target_include_directories(LCI PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
