#ifndef LCI_LCI_UCX_API_H
#define LCI_LCI_UCX_API_H
#include "ucm/api/ucm.h"
#include "ucs/memory/rcache.h"

#define LCII_API __attribute__((visibility("default")))

LCII_API ucs_stats_node_t* LCII_ucs_stats_get_root(void);

LCII_API ucs_status_t LCII_ucs_rcache_create(const ucs_rcache_params_t* params,
                                             const char* name,
                                             ucs_stats_node_t* stats_parent,
                                             ucs_rcache_t** rcache_p);

LCII_API void LCII_ucs_rcache_destroy(ucs_rcache_t* rcache);

LCII_API ucs_status_t LCII_ucs_rcache_get(ucs_rcache_t* rcache, void* address,
                                          size_t length, int prot, void* arg,
                                          ucs_rcache_region_t** region_p);

LCII_API void LCII_ucs_rcache_region_hold(ucs_rcache_t* rcache,
                                          ucs_rcache_region_t* region);

LCII_API void LCII_ucs_rcache_region_put(ucs_rcache_t* rcache,
                                         ucs_rcache_region_t* region);

#endif  // LCI_LCI_UCX_API_H
