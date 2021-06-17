#include "lcii.h"

LCI_error_t LCI_open()
{
  int num_proc, rank;
  // Initialize processes in this job.
  lc_pm_master_init(&num_proc, &rank);
  LCM_Init();

  // Set some constant from environment variable.
  lc_env_init(num_proc, rank);

  lc_dev_init(&LCI_UR_DEVICE);

  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_endpoint_init(&LCI_UR_ENDPOINT, LCI_UR_DEVICE, plist);
  LCI_queue_create(0, &LCI_UR_CQ);
  LCM_DBG_Log(LCM_LOG_WARN, "Macro LCI_DEBUG is defined. Running in low-performance debug mode!\n");

  return LCI_OK;
}

LCI_error_t LCI_close()
{
  LCI_queue_free(&LCI_UR_CQ);
  LCI_endpoint_free(&LCI_UR_ENDPOINT);
  lc_dev_finalize(LCI_UR_DEVICE);
  LCI_barrier();
  lc_pm_finalize();
  return LCI_OK;
}

LCI_error_t LCI_progress(LCI_device_t device)
{
  lc_server_progress(device->server);
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