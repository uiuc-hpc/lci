#include "lci.h"
#include "src/include/lci_priv.h"
#include "src/include/cq.h"

lc_server* lcg_dev[8];
LCI_endpoint_t lcg_endpoint[8];
int lcg_num_devices = 0;
int lcg_num_endpoints= 0;
int lcg_rank = 0;
int lcg_size = 0;
char lcg_name[64];

int lcg_current_id = 0;
int lcg_deadlock = 0;
volatile uint32_t next_key = 1;
__thread int lcg_core_id = -1;

LCI_error_t LCI_initialize(int num_devices)
{
  // Initialize processes in this job.
  lc_pm_master_init(&lcg_size, &lcg_rank, lcg_name);

  for (int i = 0; i < num_devices; i++) {
    lc_dev_init(i, &lcg_dev[i]);
  }
  lcg_num_devices = num_devices;
  return LCI_OK;
}

LCI_error_t LCI_finalize()
{
  for (int i = 0; i < lcg_num_devices; i++) {
    lc_dev_finalize(lcg_dev[i]);
  }
  return LCI_OK;
}

LCI_error_t LCI_endpoint_create(int device, LCI_PL prop, LCI_endpoint_t* ep_ptr)
{
  struct LCI_endpoint_s* ep = 0;
  lc_server* dev = lcg_dev[device];
  posix_memalign((void**) &ep, 64, sizeof(struct LCI_endpoint_s));
  ep->server = dev;
  ep->pkpool = dev->pkpool;
  ep->gid = lcg_num_endpoints++;
  lcg_endpoint[ep->gid] = ep;

  if (prop->ctype == LCI_COMM_2SIDED || prop->ctype == LCI_COMM_COLLECTIVE) {
    lc_hash_create(&ep->tbl);
    ep->property = EP_AR_EXP;
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
    lc_cq_create(&ep->cq);
  }

  ep->rep = dev->rep;
  *ep_ptr = ep;
  return LCI_OK;
}

LCI_error_t LCI_PL_create(LCI_PL* prop_ptr)
{
  struct LCI_PL_s* prop = 0;
  posix_memalign((void**) &prop, 64, sizeof(struct LCI_PL_s));
  prop->ctype = LCI_COMM_COLLECTIVE;
  prop->mtype = LCI_MSG_DIRECT;
  prop->rtype = LCI_COMPLETION_ONE2ONEL;
  prop->ltype = LCI_COMPLETION_ONE2ONEL;

  *prop_ptr = prop;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_comm_type(LCI_comm_t type, LCI_PL* prop)
{
  (*prop)->ctype = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_message_type(LCI_msg_t type, LCI_PL* prop)
{
  (*prop)->mtype = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_handler(LCI_Handler handler, LCI_PL* prop)
{
  (*prop)->handler = handler;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_allocator(LCI_Allocator handler, LCI_PL* prop)
{
  (*prop)->allocator = handler;
  return LCI_OK;
}

LCI_API
LCI_error_t LCI_PL_set_sync_type(LCI_comp_t ltype, LCI_comp_t rtype, LCI_PL* prop)
{
  (*prop)->ltype = ltype;
  (*prop)->rtype = rtype;
  return LCI_OK;
}

int LCI_Rank()
{
  return lcg_rank;
}

int LCI_Size()
{
  return lcg_size;
}

LCI_error_t LCI_sync_create(void* sync) {
  *(LCI_sync_t*) sync = 0;
  return LCI_OK;
}

LCI_error_t LCI_one2one_set_full(void* sync) {
  *(LCI_sync_t*)sync = 1;
  return LCI_OK;
}

LCI_error_t LCI_one2one_wait_empty(void* sync) {
  while (*(LCI_sync_t*)sync)
    ;
  return LCI_OK;
}

int LCI_one2one_test_empty(void* sync) {
  return (*(LCI_sync_t*)sync == 0);
}

LCI_error_t LCI_one2one_set_empty(void* sync) {
  *(LCI_sync_t*)sync = 0;
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
  lc_server_progress(lcg_dev[id]);
  return LCI_OK;
}

uintptr_t LCI_get_base_addr(int id) {
  return (uintptr_t) lcg_dev[id]->heap;
}
