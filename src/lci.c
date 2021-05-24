#include "lcii.h"
#include "cq.h"

int lcg_deadlock = 0;
volatile uint32_t lc_next_rdma_key = 1;

LCI_error_t LCI_open()
{
  int num_proc, rank;
  // Initialize processes in this job.
  lc_pm_master_init(&num_proc, &rank);

  // Set some constant from environment variable.
  lc_env_init(num_proc, rank);

  for (int i = 0; i < LCI_NUM_DEVICES; i++) {
    lc_dev_init(i, &LCI_DEVICES[i], &LCI_PLISTS[i]);
  }

  LCI_endpoint_init(&LCI_UR_ENDPOINT, 0, LCI_PLISTS[0]);
  LCI_queue_create(0, &LCI_UR_CQ);
  LCM_DBG_Log(LCM_LOG_WARN, "Macro LCI_DEBUG is defined. Running in low-performance debug mode!\n");

  return LCI_OK;
}

LCI_error_t LCI_close()
{
  LCI_queue_free(&LCI_UR_CQ);
  LCI_endpoint_free(&LCI_UR_ENDPOINT);
  for (int i = 0; i < LCI_NUM_DEVICES; i++) {
    lc_dev_finalize(LCI_DEVICES[i]);
  }
  LCI_barrier();
  lc_pm_finalize();
  return LCI_OK;
}

LCI_error_t LCI_progress(int id, int count)
{
  lc_server_progress(LCI_DEVICES[id]);
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