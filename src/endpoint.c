#include "lcii.h"

LCI_endpoint_t* LCI_ENDPOINTS;

LCI_error_t LCI_endpoint_init(LCI_endpoint_t* ep_ptr, LCI_device_t device,
                              LCI_plist_t plist)
{
  static int num_endpoints = 0;
  LCI_endpoint_t ep = LCIU_malloc(sizeof(struct LCI_endpoint_s));
  LCM_Assert(num_endpoints < LCI_MAX_ENDPOINTS, "Too many endpoints!\n");
  ep->gid = num_endpoints++;
  LCI_ENDPOINTS[ep->gid] = ep;
  *ep_ptr = ep;

  lc_server* dev = LCI_DEVICES[device];
  ep->device = device;
  ep->server = dev;
  ep->pkpool = dev->pkpool;
  ep->rep = dev->rep;
  ep->mt = dev->mt;
  LCM_archive_init(&(ep->ctx_archive), 16);

  ep->match_type = plist->match_type;
  ep->cmd_comp_type = plist->cmd_comp_type;
  ep->msg_comp_type = plist->msg_comp_type;
  ep->allocator = plist->allocator;

  return LCI_OK;
}

LCI_error_t LCI_endpoint_free(LCI_endpoint_t* ep_ptr)
{
  LCI_endpoint_t ep = *ep_ptr;

  LCM_archive_fini(&(ep->ctx_archive));
  LCI_ENDPOINTS[ep->gid] = NULL;
  LCIU_free(ep);

  *ep_ptr = NULL;
  return LCI_OK;
}