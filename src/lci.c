#include "lcii.h"

int g_server_no_recv_packets;
static int opened = 0;

LCI_error_t LCI_initialize()
{
  int num_proc, rank;
  // Initialize processes in this job.
  lcm_pm_initialize();
  rank = lcm_pm_get_rank();
  num_proc = lcm_pm_get_size();
  LCM_Init();

  // Set some constant from environment variable.
  lc_env_init(num_proc, rank);

  lc_dev_init(&LCI_UR_DEVICE);

  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_endpoint_init(&LCI_UR_ENDPOINT, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);
  LCI_queue_create(LCI_UR_DEVICE, &LCI_UR_CQ);
  LCM_DBG_Log(LCM_LOG_WARN, "Macro LCI_DEBUG is defined. Running in low-performance debug mode!\n");

  opened = 1;

  return LCI_OK;
}

LCI_error_t LCI_initialized(int *flag) {
  *flag = opened;
  return LCI_OK;
}

LCI_error_t LCI_finalize()
{
  LCI_barrier();
  LCI_queue_free(&LCI_UR_CQ);
  LCI_endpoint_free(&LCI_UR_ENDPOINT);
  lc_dev_finalize(LCI_UR_DEVICE);
  lcm_pm_finalize();

  opened = 0;
  return LCI_OK;
}

LCI_error_t LCI_progress(LCI_device_t device)
{
  lc_server_progress(device->server);
  return LCI_OK;
}

void LCI_barrier() {
  lcm_pm_barrier();
}

int LCI_Rank()
{
  return LCI_RANK;
}

int LCI_Size()
{
  return LCI_NUM_PROCESSES;
}