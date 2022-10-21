#include "lcii.h"

static int opened = 0;

LCI_error_t LCI_initialize()
{
#ifdef LCI_USE_HANG_DETECTOR
  LCII_hang_detector_init();
#endif
  int num_proc, rank;
  // Initialize processes in this job.
  lcm_pm_initialize();
  rank = lcm_pm_get_rank();
  num_proc = lcm_pm_get_size();
  LCM_Init(rank);

  // Set some constant from environment variable.
  lc_env_init(num_proc, rank);

  LCI_device_init(&LCI_UR_DEVICE);

  LCI_queue_create(LCI_UR_DEVICE, &LCI_UR_CQ);
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_endpoint_init(&LCI_UR_ENDPOINT, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);
  LCM_DBG_Warn("Macro LCI_DEBUG is defined. Running in low-performance debug mode!\n");

  opened = 1;
  LCI_barrier();
  return LCI_OK;
}

LCI_error_t LCI_initialized(int *flag) {
  *flag = opened;
  return LCI_OK;
}

LCI_error_t LCI_finalize()
{
  LCI_barrier();
  LCI_endpoint_free(&LCI_UR_ENDPOINT);
  LCI_queue_free(&LCI_UR_CQ);
  LCI_device_free(&LCI_UR_DEVICE);
  LCM_Fina();
  lcm_pm_finalize();
#ifdef LCI_USE_HANG_DETECTOR
  LCII_hang_detector_fina();
#endif

  opened = 0;
  return LCI_OK;
}

LCI_error_t LCI_barrier() {
  lcm_pm_barrier();
  return LCI_OK;
}