#include "lci.h"
#include "src/include/lci_priv.h"
#include "src/include/cq.h"

lc_server** LCI_DEVICES;
LCI_endpoint_t* LCI_ENDPOINTS;

char lcg_name[64];
int lcg_current_id = 0; int lcg_deadlock = 0;
volatile uint32_t lc_next_rdma_key = 1;
__thread int lcg_core_id = -1;

void lc_config_init(int num_proc, int rank);

LCI_error_t LCI_initialize(int* argc, char*** args)
{
  int num_proc, rank;
  // Initialize processes in this job.
  lc_pm_master_init(&num_proc, &rank, lcg_name);

  // Set some constant from environment variable.
  lc_config_init(num_proc, rank);

  for (int i = 0; i < LCI_NUM_DEVICES; i++) {
    lc_dev_init(i, &LCI_DEVICES[i]);
  }
  return LCI_OK;
}

LCI_error_t LCI_finalize()
{
  for (int i = 0; i < LCI_NUM_DEVICES; i++) {
    lc_dev_finalize(LCI_DEVICES[i]);
  }
  LCI_PM_barrier();
  return LCI_OK;
}

LCI_error_t LCI_endpoint_create(int device, LCI_PL_t prop, LCI_endpoint_t* ep_ptr)
{
  static int num_endpoints = 0;
  struct LCI_endpoint_s* ep = 0;
  lc_server* dev = LCI_DEVICES[device];
  posix_memalign((void**) &ep, 64, sizeof(struct LCI_endpoint_s));
  ep->server = dev;
  ep->pkpool = dev->pkpool;
  ep->gid = num_endpoints++;
  LCI_ENDPOINTS[ep->gid] = ep;

  if (prop->ctype == LCI_COMM_2SIDED || prop->ctype == LCI_COMM_COLLECTIVE) {
    ep->property = EP_AR_EXP;
    ep->mt = (lc_hash*) prop->mt;
  } else {
    ep->property = EP_AR_DYN;
    ep->alloc = prop->allocator;
  }

  if (prop->rtype == LCI_COMPLETION_ONE2ONEL) {
    ep->property |= EP_CE_SYNC;
  } else if (prop->rtype == LCI_COMPLETION_HANDLER) {
    ep->property |= EP_CE_AM;
    ep->handler = prop->handler;
  } else if (prop->rtype == LCI_COMPLETION_QUEUE) {
    ep->property |= EP_CE_CQ;
    ep->cq = (lc_cq*) prop->cq;
  }

  ep->rep = dev->rep;
  *ep_ptr = ep;
  return LCI_OK;
}

LCI_error_t LCI_cq_dequeue(LCI_endpoint_t ep, LCI_request_t** req_ptr)
{
  LCI_request_t* req = lc_cq_pop(ep->cq);
  if (!req) return LCI_ERR_RETRY;
  *req_ptr = req;
  return LCI_OK;
}

LCI_error_t LCI_request_free(LCI_endpoint_t ep, int n, LCI_request_t** req)
{
  lc_packet* packet = (lc_packet*) ((*req)->__reserved__);
  lc_pool_put(ep->pkpool, packet);
  return LCI_OK;
}

LCI_error_t LCI_progress(int id, int count)
{
  lc_server_progress(LCI_DEVICES[id]);
  return LCI_OK;
}

uintptr_t LCI_get_base_addr(int id) {
  return (uintptr_t) LCI_DEVICES[id]->heap_addr;
}

LCI_error_t LCI_buffer_get(LCI_endpoint_t ep, LCI_bdata_t* buffer) {
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  *buffer = (void*) p;
  return LCI_OK;
}

LCI_error_t LCI_buffer_free(LCI_endpoint_t ep, LCI_bdata_t buffer)
{
  lc_packet* packet = (lc_packet*) buffer;
  lc_pool_put(ep->pkpool, packet);
  return LCI_OK;
}