#include "lci_ucx_api.h"

ucs_stats_node_t* LCII_ucs_stats_get_root(void) { return ucs_stats_get_root(); }

ucs_status_t LCII_ucs_rcache_create(const ucs_rcache_params_t* params,
                                    const char* name,
                                    ucs_stats_node_t* stats_parent,
                                    ucs_rcache_t** rcache_p)
{
  return ucs_rcache_create(params, name, stats_parent, rcache_p);
}

void LCII_ucs_rcache_destroy(ucs_rcache_t* rcache)
{
  ucs_rcache_destroy(rcache);
}

ucs_status_t LCII_ucs_rcache_get(ucs_rcache_t* rcache, void* address,
                                 size_t length, int prot, void* arg,
                                 ucs_rcache_region_t** region_p)
{
  return ucs_rcache_get(rcache, address, length, prot, arg, region_p);
}

void LCII_ucs_rcache_region_hold(ucs_rcache_t* rcache,
                                 ucs_rcache_region_t* region)
{
  ucs_rcache_region_hold(rcache, region);
}

void LCII_ucs_rcache_region_put(ucs_rcache_t* rcache,
                                ucs_rcache_region_t* region)
{
  ucs_rcache_region_put(rcache, region);
}