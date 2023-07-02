#include <limits.h>
#include "runtime/lcii.h"

LCI_API int LCI_NUM_PROCESSES;
LCI_API int LCI_RANK;
LCI_API int LCI_MAX_ENDPOINTS;
LCI_API int LCI_MAX_TAG = (1u << 16) - 1;
LCI_API int LCI_MEDIUM_SIZE = -1;
LCI_API int LCI_IOVEC_SIZE = -1;
LCI_API int LCI_DEFAULT_QUEUE_LENGTH;
LCI_API int LCI_MAX_QUEUE_LENGTH;
LCI_API int LCI_MAX_SYNC_LENGTH = INT_MAX;
LCI_API int LCI_PACKET_RETURN_THRESHOLD;
LCI_API int LCI_IBV_USE_ODP;
LCI_API int LCI_TOUCH_LBUFFER;
LCI_API int LCI_USE_DREG;
LCI_API int LCI_IBV_USE_PREFETCH;
LCI_API int LCI_PACKET_SIZE;
LCI_API int LCI_SERVER_NUM_PKTS;
LCI_API int LCI_SERVER_MAX_SENDS;
LCI_API int LCI_SERVER_MAX_RECVS;
LCI_API int LCI_SERVER_MAX_CQES;
LCI_API uint64_t LCI_PAGESIZE;
LCI_API bool LCI_IBV_ENABLE_EVENT_POLLING_THREAD;
LCI_API int LCI_SEND_SLOW_DOWN_USEC;
LCI_API int LCI_RECV_SLOW_DOWN_USEC;
LCI_API LCI_device_t LCI_UR_DEVICE;
LCI_API LCI_endpoint_t LCI_UR_ENDPOINT;
LCI_API LCI_comp_t LCI_UR_CQ;

void LCII_env_init_cq_type();

void LCII_env_init(int num_proc, int rank)
{
  LCI_PAGESIZE = sysconf(_SC_PAGESIZE);

  LCI_MAX_ENDPOINTS = LCIU_getenv_or("LCI_MAX_ENDPOINTS", 8);
  LCI_NUM_PROCESSES = num_proc;
  LCI_RANK = rank;
  LCI_ENDPOINTS = LCIU_calloc(sizeof(LCI_endpoint_t), LCI_MAX_ENDPOINTS);

  LCI_PACKET_RETURN_THRESHOLD =
      LCIU_getenv_or("LCI_PACKET_RETURN_THRESHOLD", 1024);
  LCI_IBV_USE_ODP = LCIU_getenv_or("LCI_IBV_USE_ODP", 0);
  LCI_TOUCH_LBUFFER = LCIU_getenv_or("LCI_TOUCH_LBUFFER", 0);
  LCI_USE_DREG = LCIU_getenv_or("LCI_USE_DREG", LCI_USE_DREG_DEFAULT);
  if (LCI_USE_DREG && LCI_IBV_USE_ODP == 2) {
    LCI_Warn(
        "It doesn't make too much sense to use registration cache "
        "with implicit on-demand paging\n");
  }
  LCI_IBV_USE_PREFETCH = LCIU_getenv_or("LCI_IBV_USE_PREFETCH", 0);
  if (LCI_IBV_USE_PREFETCH && LCI_IBV_USE_ODP == 0) {
    LCI_Warn(
        "It doesn't make too much sense to use prefetch "
        "without on-demand paging\n");
  }
  LCI_PACKET_SIZE = LCIU_getenv_or("LCI_PACKET_SIZE", LCI_PACKET_SIZE_DEFAULT);
  LCI_SERVER_NUM_PKTS =
      LCIU_getenv_or("LCI_SERVER_NUM_PKTS", LCI_SERVER_NUM_PKTS_DEFAULT);
  LCI_DEFAULT_QUEUE_LENGTH =
      LCIU_getenv_or("LCI_DEFAULT_QUEUE_LENGTH", LCI_SERVER_NUM_PKTS * 2);
  LCI_MAX_QUEUE_LENGTH = LCI_DEFAULT_QUEUE_LENGTH;
  LCI_SERVER_MAX_SENDS =
      LCIU_getenv_or("LCI_SERVER_MAX_SENDS", LCI_SERVER_MAX_SENDS_DEFAULT);
  LCI_SERVER_MAX_RECVS =
      LCIU_getenv_or("LCI_SERVER_MAX_RECVS", LCI_SERVER_MAX_RECVS_DEFAULT);
  LCI_SERVER_MAX_CQES =
      LCIU_getenv_or("LCI_SERVER_MAX_CQES", LCI_SERVER_MAX_CQES_DEFAULT);
  LCI_IBV_ENABLE_EVENT_POLLING_THREAD =
      LCIU_getenv_or("LCI_IBV_ENABLE_EVENT_POLLING_THREAD", false);
#ifdef LCI_ENABLE_SLOWDOWN
  LCI_SEND_SLOW_DOWN_USEC = LCIU_getenv_or("LCI_SEND_SLOW_DOWN_USEC", 0);
  LCI_RECV_SLOW_DOWN_USEC = LCIU_getenv_or("LCI_RECV_SLOW_DOWN_USEC", 0);
#else
  LCI_SEND_SLOW_DOWN_USEC = 0;
  LCI_RECV_SLOW_DOWN_USEC = 0;
#endif
  LCI_MEDIUM_SIZE = LCI_PACKET_SIZE - sizeof(struct LCII_packet_context);
  LCI_IOVEC_SIZE =
      LCIU_MIN((LCI_PACKET_SIZE - sizeof(struct LCII_packet_context) -
                sizeof(struct LCII_packet_rts_t)) /
                   sizeof(size_t),
               (LCI_PACKET_SIZE - sizeof(struct LCII_packet_context) -
                sizeof(struct LCII_packet_rtr_t)) /
                   sizeof(struct LCII_packet_rtr_iovec_info_t));

  LCII_env_init_cq_type();
}