#include "runtime/lcii.h"
#include "lci_ucx_api.h"

static int opened = 0;
int LCIU_nthreads = 0;
__thread int LCIU_thread_id = -1;
__thread unsigned int LCIU_rand_seed = 0;

LCI_error_t LCI_initialize()
{
  LCT_init();
  LCII_log_init();
  // Initialize PMI.
  int num_proc, rank;
  lcm_pm_initialize();
  rank = lcm_pm_get_rank();
  num_proc = lcm_pm_get_size();
  LCT_set_rank(rank);
  LCII_pcounters_init();
  // Set some constant from environment variable.
  LCII_env_init(num_proc, rank);
  LCII_papi_init();
  if (LCI_USE_DREG) {
    LCII_ucs_init();
  }

  LCI_device_init(&LCI_UR_DEVICE);

  LCI_queue_create(LCI_UR_DEVICE, &LCI_UR_CQ);
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_endpoint_init(&LCI_UR_ENDPOINT, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);
  LCI_DBG_Warn(
      "Macro LCI_DEBUG is defined. Running in low-performance debug mode!\n");

  opened = 1;
  LCI_barrier();
  LCI_Log(LCI_LOG_INFO, "device", "LCI_initialize is called\n");
  return LCI_OK;
}

LCI_error_t LCI_initialized(int* flag)
{
  *flag = opened;
  return LCI_OK;
}

LCI_error_t LCI_finalize()
{
  LCI_Log(LCI_LOG_INFO, "device", "LCI_finalize is called\n");
  LCI_barrier();
  LCII_papi_fina();
  LCI_endpoint_free(&LCI_UR_ENDPOINT);
  LCI_queue_free(&LCI_UR_CQ);
  LCI_device_free(&LCI_UR_DEVICE);
  if (LCI_USE_DREG) {
    LCII_ucs_cleanup();
  }
  lcm_pm_finalize();
  LCII_pcounters_fina();
  LCII_log_fina();
  LCT_fina();

  opened = 0;
  return LCI_OK;
}

// This function is not thread-safe.
LCI_error_t LCII_barrier()
{
  if (LCI_NUM_PROCESSES <= 1) return LCI_OK;

  static LCI_tag_t next_tag = 0;
  static LCI_endpoint_t ep = NULL;
  if (ep == NULL) {
    LCI_plist_t plist;
    LCI_plist_create(&plist);
    LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_SYNC);
    LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
    LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);
    LCI_plist_free(&plist);
  }
  LCI_tag_t tag = next_tag++;
  LCI_Log(LCI_LOG_INFO, "coll", "Start barrier (%d, %p).\n", tag, ep);
  LCI_mbuffer_t buffer;
  int nonsense;
  buffer.address = &nonsense;
  buffer.length = sizeof(nonsense);

  if (LCI_RANK != 0) {
    // Other ranks
    // Phase 1: all the other ranks send a message to rank 0.
    while (LCI_sendm(ep, buffer, 0, tag) != LCI_OK) {
      LCI_progress(LCI_UR_DEVICE);
    }
    // Phase 2: rank 0 send a message to all the other ranks.
    LCI_comp_t sync;
    LCI_sync_create(LCI_UR_DEVICE, 1, &sync);
    LCI_recvm(ep, buffer, 0, tag, sync, NULL);
    while (LCI_sync_test(sync, NULL) != LCI_OK) {
      LCI_progress(LCI_UR_DEVICE);
    }
    LCI_sync_free(&sync);
  } else {
    // rank 0
    // Phase 1: all the other ranks send a message to rank 0.
    LCI_comp_t sync;
    LCI_sync_create(LCI_UR_DEVICE, LCI_NUM_PROCESSES - 1, &sync);
    for (int i = 1; i < LCI_NUM_PROCESSES; ++i) {
      LCI_recvm(ep, buffer, i, tag, sync, NULL);
    }
    while (LCI_sync_test(sync, NULL) != LCI_OK) {
      LCI_progress(LCI_UR_DEVICE);
    }
    LCI_sync_free(&sync);
    // Phase 2: rank 0 send a message to all the other ranks.
    for (int i = 1; i < LCI_NUM_PROCESSES; ++i) {
      while (LCI_sendm(ep, buffer, i, tag) != LCI_OK)
        LCI_progress(LCI_UR_DEVICE);
    }
  }
  LCI_Log(LCI_LOG_INFO, "coll", "End barrier (%d, %p).\n", tag, ep);
  return LCI_OK;
}

LCI_error_t LCI_barrier()
{
  //  lcm_pm_barrier();
  //  return LCI_OK;
  return LCII_barrier();
}