#include <limits.h>
#include "lci.h"
#include "lcii.h"
#include "cq.h"

lc_server** LCI_DEVICES;
LCI_endpoint_t* LCI_ENDPOINTS;

char lcg_name[256];
int lcg_deadlock = 0;
volatile uint32_t lc_next_rdma_key = 1;

LCI_API int LCI_NUM_DEVICES;
LCI_API int LCI_NUM_PROCESSES;
LCI_API int LCI_RANK;
LCI_API int LCI_MAX_ENDPOINTS;
LCI_API int LCI_MAX_TAG = (1u << 15) - 1;
LCI_API int LCI_MEDIUM_SIZE = LC_PACKET_SIZE - sizeof(struct packet_context);
LCI_API int LCI_REGISTERED_SEGMENT_SIZE;
LCI_API int LCI_MAX_REGISTERED_SEGMENT_SIZE = INT_MAX;
LCI_API int LCI_MAX_REGISTERED_SEGMENT_NUMBER = 1;
LCI_API int LCI_DEFAULT_TABLE_LENGTH = 1u << TBL_BIT_SIZE;
LCI_API int LCI_MAX_TABLE_LENGTH = 1u << TBL_BIT_SIZE;
LCI_API int LCI_DEFAULT_QUEUE_LENGTH = CQ_MAX_SIZE;
LCI_API int LCI_MAX_QUEUE_LENGTH = CQ_MAX_SIZE;
LCI_API int LCI_LOG_LEVEL = LCI_LOG_WARN;
LCI_API int LCI_PACKET_RETURN_THRESHOLD;

static inline int getenv_or(char* env, int def) {
  char* val = getenv(env);
  if (val != NULL) {
    return atoi(val);
  } else {
    return def;
  }
}

void lc_config_init(int num_proc, int rank)
{
  char *p;

  LCI_NUM_DEVICES = getenv_or("LCI_NUM_DEVICES", 1);
  LCI_MAX_ENDPOINTS = getenv_or("LCI_MAX_ENDPOINTS", 8);
  LCI_NUM_PROCESSES = num_proc;
  LCI_RANK = rank;
  LCI_REGISTERED_SEGMENT_SIZE = getenv_or("LCI_REGISTERED_SEGMENT_SIZE", LC_DEV_MEM_SIZE);

  p = getenv("LCI_LOG_LEVEL");
  if (p == NULL) ;
  else if (strcmp(p, "none") == 0 || strcmp(p, "NONE") == 0)
    LCI_LOG_LEVEL = LCI_LOG_NONE;
  else if (strcmp(p, "warn") == 0 || strcmp(p, "WARN") == 0)
    LCI_LOG_LEVEL = LCI_LOG_WARN;
  else if (strcmp(p, "trace") == 0 || strcmp(p, "TRACE") == 0)
    LCI_LOG_LEVEL = LCI_LOG_TRACE;
  else if (strcmp(p, "info") == 0 || strcmp(p, "INFO") == 0)
    LCI_LOG_LEVEL = LCI_LOG_INFO;
  else if (strcmp(p, "debug") == 0 || strcmp(p, "DEBUG") == 0)
    LCI_LOG_LEVEL = LCI_LOG_DEBUG;
  else if (strcmp(p, "max") == 0 || strcmp(p, "MAX") == 0)
    LCI_LOG_LEVEL = LCI_LOG_MAX;
  else
    LCI_Log(LCI_LOG_WARN, "unknown env LCI_LOG_LEVEL (%s against none|warn|trace|info|debug|max). use the default LCI_LOG_WARN.\n", p);

  LCI_DEVICES = calloc(sizeof(lc_server*), LCI_NUM_DEVICES);
  LCI_ENDPOINTS = calloc(sizeof(LCI_endpoint_t), LCI_MAX_ENDPOINTS);

  LCI_PACKET_RETURN_THRESHOLD = getenv_or("LCI_PACKET_RETURN_THRESHOLD", 1024);
}

void lc_dev_init(int id, lc_server** dev)
{
  uintptr_t base_packet;
  lc_server_init(id, dev);
  lc_server* s = *dev;
  uintptr_t base_addr = s->heap_addr;
  base_packet = base_addr + 8192 - sizeof(struct packet_context);

  lc_pool_create(&s->pkpool);
  for (int i = 0; i < LC_SERVER_NUM_PKTS; i++) {
    lc_packet* p = (lc_packet*)(base_packet + i * LC_PACKET_SIZE);
    p->context.pkpool = s->pkpool;
    p->context.poolid = 0;
    lc_pool_put(s->pkpool, p);
  }
}

void lc_dev_finalize(lc_server* dev)
{
  lc_pool_destroy(dev->pkpool);
  lc_server_finalize(dev);
}

LCI_error_t LCI_open()
{
  int num_proc, rank;
  // Initialize processes in this job.
  lc_pm_master_init(&num_proc, &rank, lcg_name);

  // Set some constant from environment variable.
  lc_config_init(num_proc, rank);

  for (int i = 0; i < LCI_NUM_DEVICES; i++) {
    lc_dev_init(i, &LCI_DEVICES[i]);
  }

  LCI_DBG_Log(LCI_LOG_WARN, "Macro LCI_DEBUG is defined. Running in low-performance debug mode!\n");

  return LCI_OK;
}

LCI_error_t LCI_close()
{
  for (int i = 0; i < LCI_NUM_DEVICES; i++) {
    lc_dev_finalize(LCI_DEVICES[i]);
  }
  LCI_barrier();
  lc_pm_finalize();
  return LCI_OK;
}

LCI_error_t LCI_endpoint_create(int device, LCI_plist_t plist, LCI_endpoint_t* ep_ptr)
{
  static int num_endpoints = 0;
  struct LCI_endpoint_s* ep = LCIU_malloc(sizeof(struct LCI_endpoint_s));
  lc_server* dev = LCI_DEVICES[device];
  ep->server = dev;
  ep->pkpool = dev->pkpool;
  ep->gid = num_endpoints++;
  LCII_register_init(&(ep->ctx_reg), 16);
  LCI_Assert(num_endpoints < LCI_MAX_ENDPOINTS, "Too many endpoints!\n");
  LCI_ENDPOINTS[ep->gid] = ep;

  if (plist->ctype == LCI_COMM_2SIDED || plist->ctype == LCI_COMM_COLLECTIVE) {
    ep->property = EP_AR_EXP;
    ep->mt = (lc_hash*) plist->mt;
  } else {
    ep->property = EP_AR_DYN;
    ep->alloc = plist->allocator;
  }

  if (plist->rtype == LCI_COMPLETION_ONE2ONEL) {
    ep->property |= EP_CE_SYNC;
  } else if (plist->rtype == LCI_COMPLETION_HANDLER) {
    ep->property |= EP_CE_AM;
    ep->handler = plist->handler;
  } else if (plist->rtype == LCI_COMPLETION_QUEUE) {
    ep->property |= EP_CE_CQ;
    ep->cq = (lc_cq*) plist->cq;
  }

  ep->rep = dev->rep;
  *ep_ptr = ep;
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

LCI_error_t LCI_bbuffer_get(LCI_mbuffer_t* buffer, int device_id) {
  LC_POOL_GET_OR_RETN(LCI_DEVICES[device_id]->pkpool, p);
  *buffer = (void*) &p->data;
  return LCI_OK;
}

LCI_error_t LCI_bbuffer_free(LCI_mbuffer_t buffer, int device_id)
{
  lc_packet* packet = LCII_mbuffer2packet(buffer);
  lc_pool_put(LCI_DEVICES[device_id]->pkpool, packet);
  return LCI_OK;
}

void LCI_barrier() {
  lc_pm_barrier();
}

int LCI_Rank()
{
  return LCI_RANK;
}

int LCI_Size()
{
  return LCI_NUM_PROCESSES;
}