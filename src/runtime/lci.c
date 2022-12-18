#include "runtime/lcii.h"
#include "lci_ucx_api.h"

static int opened = 0;
int LCIU_nthreads = 0;
__thread int LCIU_thread_id = -1;
__thread unsigned int LCIU_rand_seed = 0;

LCI_error_t LCI_initialize()
{
  int num_proc, rank;
  // Initialize processes in this job.
  lcm_pm_initialize();
  rank = lcm_pm_get_rank();
  num_proc = lcm_pm_get_size();
  LCM_Init(rank);
  // Set some constant from environment variable.
  LCII_env_init(num_proc, rank);
  LCII_pcounters_init();
  LCII_monitor_thread_init();
  if (LCI_USE_DREG) {
    LCII_ucs_init();
  }

  LCI_device_init(&LCI_UR_DEVICE);

  LCI_queue_create(LCI_UR_DEVICE, &LCI_UR_CQ);
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_endpoint_init(&LCI_UR_ENDPOINT, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);
  LCM_DBG_Warn(
      "Macro LCI_DEBUG is defined. Running in low-performance debug mode!\n");

  opened = 1;
  LCI_barrier();
  return LCI_OK;
}

LCI_error_t LCI_initialized(int* flag)
{
  *flag = opened;
  return LCI_OK;
}

LCI_error_t LCI_finalize()
{
  LCI_barrier();
  LCI_endpoint_free(&LCI_UR_ENDPOINT);
  LCI_queue_free(&LCI_UR_CQ);
  LCI_device_free(&LCI_UR_DEVICE);
  if (LCI_USE_DREG) {
    LCII_ucs_cleanup();
  }
  LCII_monitor_thread_fina();
  LCM_Fina();
  lcm_pm_finalize();

  opened = 0;
  return LCI_OK;
}

LCI_error_t LCI_barrier()
{
  lcm_pm_barrier();
  return LCI_OK;
}