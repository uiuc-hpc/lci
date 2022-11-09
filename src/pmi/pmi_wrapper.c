#include "pmi_wrapper.h"
#include <stdlib.h>
#include <string.h>

struct LCM_PM_ops_t LCM_PM_ops;

void lcm_pm_initialize()
{
  {
    char* p = getenv("LCI_PM_BACKEND");
    if (p == NULL) p = LCI_PM_BACKEND_DEFAULT;
    if (strcmp(p, "pmi1") == 0) {
      lcm_pm_pmi1_setup_ops(&LCM_PM_ops);
    } else if (strcmp(p, "pmi2") == 0) {
      lcm_pm_pmi2_setup_ops(&LCM_PM_ops);
    }
#ifdef LCI_PM_BACKEND_ENABLE_MPI
    else if (strcmp(p, "mpi") == 0) {
      lcm_pm_mpi_setup_ops(&LCM_PM_ops);
    }
#endif
    else
      LCM_Assert(false,
                 "unknown env LCM_PM_BACKEND (%s against pmi1|pmi2"
#ifdef LCI_PM_BACKEND_ENABLE_MPI
                 "|mpi"
#endif
                 ").\n",
                 p);
  }

  LCM_PM_ops.initialize();
}
int lcm_pm_initialized() { return LCM_PM_ops.is_initialized(); }
int lcm_pm_get_rank() { return LCM_PM_ops.get_rank(); }
int lcm_pm_get_size() { return LCM_PM_ops.get_size(); }
void lcm_pm_publish(char* key, char* value) { LCM_PM_ops.publish(key, value); }
void lcm_pm_getname(char* key, char* value) { LCM_PM_ops.getname(key, value); }
void lcm_pm_barrier() { LCM_PM_ops.barrier(); }
void lcm_pm_finalize() { LCM_PM_ops.finalize(); }