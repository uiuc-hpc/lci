#include <limits.h>
#include "lcii.h"

LCI_API int LCI_NUM_PROCESSES;
LCI_API int LCI_RANK;
LCI_API int LCI_MAX_ENDPOINTS;
LCI_API int LCI_MAX_TAG = (1u << 15) - 1;
LCI_API int LCI_MEDIUM_SIZE = LCI_PACKET_SIZE - sizeof(struct packet_context);
LCI_API int LCI_IOVEC_SIZE =
    LCIU_MIN((LCI_PACKET_SIZE - sizeof(struct packet_context) -
              sizeof(struct packet_rts)) /
                 sizeof(size_t),
             (LCI_PACKET_SIZE - sizeof(struct packet_context) -
              sizeof(struct packet_rtr)) /
                 sizeof(struct packet_rtr_iovec_info));
LCI_API int LCI_REGISTERED_SEGMENT_SIZE;
LCI_API int LCI_MAX_REGISTERED_SEGMENT_SIZE = INT_MAX;
LCI_API int LCI_MAX_REGISTERED_SEGMENT_NUMBER = 1;
LCI_API int LCI_DEFAULT_TABLE_LENGTH = 1u << TBL_BIT_SIZE;
LCI_API int LCI_MAX_TABLE_LENGTH = INT32_MAX;
LCI_API int LCI_DEFAULT_QUEUE_LENGTH;
LCI_API int LCI_MAX_QUEUE_LENGTH;
LCI_API int LCI_MAX_SYNC_LENGTH = INT_MAX;
LCI_API int LCI_PACKET_RETURN_THRESHOLD;
LCI_API int LCI_IBV_USE_ODP;
LCI_API int LCI_TOUCH_LBUFFER;
LCI_API int LCI_USE_DREG;
LCI_API int LCI_IBV_USE_PREFETCH;
LCI_API int LCI_SERVER_NUM_PKTS;
LCI_API int LCI_SERVER_MAX_SENDS;
LCI_API int LCI_SERVER_MAX_RECVS;
LCI_API int LCI_SERVER_MAX_CQES;
LCI_API uint64_t LCI_PAGESIZE;
LCI_API bool LCI_IBV_ENABLE_EVENT_POLLING_THREAD;
LCI_API LCI_device_t LCI_UR_DEVICE;
LCI_API LCI_endpoint_t LCI_UR_ENDPOINT;
LCI_API LCI_comp_t LCI_UR_CQ;

void lc_env_init(int num_proc, int rank)
{
  LCI_PAGESIZE = sysconf(_SC_PAGESIZE);

  LCI_MAX_ENDPOINTS = getenv_or("LCI_MAX_ENDPOINTS", 8);
  LCI_NUM_PROCESSES = num_proc;
  LCI_RANK = rank;
  LCI_REGISTERED_SEGMENT_SIZE =
      getenv_or("LCI_REGISTERED_SEGMENT_SIZE", LCI_DEV_MEM_SIZE);
  LCI_ENDPOINTS = LCIU_calloc(sizeof(LCI_endpoint_t), LCI_MAX_ENDPOINTS);

  LCI_PACKET_RETURN_THRESHOLD = getenv_or("LCI_PACKET_RETURN_THRESHOLD", 1024);
  LCI_IBV_USE_ODP = getenv_or("LCI_IBV_USE_ODP", 0);
  LCI_TOUCH_LBUFFER = getenv_or("LCI_TOUCH_LBUFFER", 0);
  LCI_USE_DREG = getenv_or("LCI_USE_DREG", LCI_USE_DREG_DEFAULT);
  if (LCI_USE_DREG && LCI_IBV_USE_ODP == 2) {
    LCM_Warn(
        "It doesn't make too much sense to use registration cache "
        "with implicit on-demand paging\n");
  }
  LCI_IBV_USE_PREFETCH = getenv_or("LCI_IBV_USE_PREFETCH", 0);
  if (LCI_IBV_USE_PREFETCH && LCI_IBV_USE_ODP == 0) {
    LCM_Warn(
        "It doesn't make too much sense to use prefetch "
        "without on-demand paging\n");
  }
  LCI_SERVER_NUM_PKTS =
      getenv_or("LCI_SERVER_NUM_PKTS", LCI_SERVER_NUM_PKTS_DEFAULT);
  LCI_DEFAULT_QUEUE_LENGTH =
      getenv_or("LCI_MAX_QUEUE_LENGTH", LCI_SERVER_NUM_PKTS * 2);
  LCI_MAX_QUEUE_LENGTH =
      getenv_or("LCI_MAX_QUEUE_LENGTH", LCI_SERVER_NUM_PKTS * 2);
  LCI_SERVER_MAX_SENDS =
      getenv_or("LCI_SERVER_MAX_SENDS", LCI_SERVER_MAX_SENDS_DEFAULT);
  LCI_SERVER_MAX_RECVS =
      getenv_or("LCI_SERVER_MAX_RECVS", LCI_SERVER_MAX_RECVS_DEFAULT);
  LCI_SERVER_MAX_CQES =
      getenv_or("LCI_SERVER_MAX_CQES", LCI_SERVER_MAX_CQES_DEFAULT);
  LCI_IBV_ENABLE_EVENT_POLLING_THREAD =
      getenv_or("LCI_IBV_ENABLE_EVENT_POLLING_THREAD", false);
}