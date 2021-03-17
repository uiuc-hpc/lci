#include <limits.h>
#include "lci.h"
#include "lcii.h"
#include "cq.h"

lc_server** LCI_DEVICES;
LCI_plist_t* LCI_PLISTS;
LCI_endpoint_t* LCI_ENDPOINTS;
LCI_endpoint_t LCI_UR_ENDPOINT;
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

  LCI_DEVICES = LCIU_calloc(sizeof(lc_server*), LCI_NUM_DEVICES);
  LCI_PLISTS = LCIU_calloc(sizeof(LCI_plist_t), LCI_NUM_DEVICES);
  LCI_ENDPOINTS = LCIU_calloc(sizeof(LCI_endpoint_t), LCI_MAX_ENDPOINTS);

  LCI_PACKET_RETURN_THRESHOLD = getenv_or("LCI_PACKET_RETURN_THRESHOLD", 1024);
}

void lc_dev_init(int id, lc_server** dev, LCI_plist_t *plist)
{
  uintptr_t base_packet;
  lc_server_init(id, dev);
  lc_server* s = *dev;
  LCI_plist_create(plist);

  LCI_MT_init(&s->mt, 0);
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
  LCI_MT_free(&dev->mt);
  lc_pool_destroy(dev->pkpool);
  lc_server_finalize(dev);
}

LCI_error_t LCI_open()
{
  int num_proc, rank;
  // Initialize processes in this job.
  lc_pm_master_init(&num_proc, &rank);

  // Set some constant from environment variable.
  lc_config_init(num_proc, rank);

  for (int i = 0; i < LCI_NUM_DEVICES; i++) {
    lc_dev_init(i, &LCI_DEVICES[i], &LCI_PLISTS[i]);
  }

  LCI_endpoint_init(&LCI_UR_ENDPOINT, 0, LCI_PLISTS[0]);
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

LCI_error_t LCI_endpoint_init(LCI_endpoint_t* ep_ptr, int device,
                              LCI_plist_t plist)
{
  static int num_endpoints = 0;
  LCI_endpoint_t ep = LCIU_malloc(sizeof(struct LCI_endpoint_s));
  LCI_Assert(num_endpoints < LCI_MAX_ENDPOINTS, "Too many endpoints!\n");
  LCI_ENDPOINTS[ep->gid] = ep;
  *ep_ptr = ep;

  lc_server* dev = LCI_DEVICES[device];
  ep->server = dev;
  ep->pkpool = dev->pkpool;
  ep->rep = dev->rep;
  ep->mt = dev->mt;
  ep->gid = num_endpoints++;
  LCII_register_init(&(ep->ctx_reg), 16);

  ep->match_type = plist->match_type;
  ep->cmd_comp_type = plist->cmd_comp_type;
  ep->msg_comp_type = plist->msg_comp_type;
  ep->allocator = plist->allocator;

  return LCI_OK;
}

LCI_error_t LCI_progress(int id, int count)
{
  lc_server_progress(LCI_DEVICES[id]);
  return LCI_OK;
}

LCI_error_t LCI_mbuffer_alloc(int device_id, LCI_mbuffer_t* mbuffer)
{
  lc_server *s = LCI_DEVICES[device_id];
  lc_packet* packet = lc_pool_get_nb(s->pkpool);
  if (packet == NULL)
    // no packet is available
    return LCI_ERR_RETRY;
  packet->context.poolid = -1;

  mbuffer->address = packet->data.address;
  mbuffer->length = LCI_MEDIUM_SIZE;
  return LCI_OK;
}

LCI_error_t LCI_mbuffer_free(int device_id, LCI_mbuffer_t mbuffer)
{
  lc_packet* packet = LCII_mbuffer2packet(mbuffer);
  LCII_free_packet(packet);
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