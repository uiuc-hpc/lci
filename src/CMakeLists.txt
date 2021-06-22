add_subdirectory(pmi)
target_include_directories(lci-obj PRIVATE include)
target_sources_relative(lci-obj PRIVATE
        1sided.c
        cq.c
        lci.c
        memory.c
        lcrq.c
        misc.c
        mt.c
        plist.c
        pool.c
        pt2pt.c
        lcm_log.c
        env.c
        device.c
        endpoint.c
        server_ofi.c
        ${CMAKE_CURRENT_SOURCE_DIR}/$<$<BOOL:${LCI_SYNC_ABT}>:sync_abt.c>
        ${CMAKE_CURRENT_SOURCE_DIR}/$<$<BOOL:${LCI_SYNC_FLAG}>:sync_flag.c>
        $<TARGET_OBJECTS:pmi-obj>
        )

if(LCI_USE_DREG)
  add_subdirectory(dreg)
endif()
add_subdirectory(include)
