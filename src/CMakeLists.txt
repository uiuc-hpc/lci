add_subdirectory(pmi)

target_include_directories(LCI PRIVATE include)
target_sources_relative(LCI PRIVATE
        1sided.c
        cq.c
        lci.c
        memory.c
        lcrq.c
        mt.c
        plist.c
        pool.c
        pt2pt.c
        lcm_log.c
        env.c
        device.c
        endpoint.c
        handler.c
        ${CMAKE_CURRENT_SOURCE_DIR}/$<$<BOOL:${LCI_SYNC_ABT}>:sync_abt.c>
        ${CMAKE_CURRENT_SOURCE_DIR}/$<$<BOOL:${LCI_SYNC_FLAG}>:sync_flag.c>
        )

if(LCI_USE_SERVER_OFI)
  target_sources_relative(LCI PRIVATE server_ofi.c)
elseif(LCI_USE_SERVER_IBV)
  target_sources_relative(LCI PRIVATE server_ibv.c)
elseif(LCI_USE_SERVER_PSM)
endif()

if(LCI_USE_DREG)
  add_subdirectory(dreg)
endif()
add_subdirectory(include)
