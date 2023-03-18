#include "pmi_wrapper.h"
#include <stdlib.h>
#include <string.h>

struct LCM_PM_ops_t LCM_PM_ops;

void lcm_pm_initialize()
{
  bool enable_log = false;
  {
    char* p = getenv("LCI_PM_BACKEND_LOG");
    if (p != NULL && strcmp(p, "0") != 0) {
      enable_log = true;
    }
  }
  char* p = getenv("LCI_PM_BACKEND");
  if (p == NULL) p = LCI_PM_BACKEND_DEFAULT;

  char* str = strdup(p);
  char* word;
  char* rest = str;
  bool found_valid_backend = false;
  while ((word = strtok_r(rest, " ;,", &rest))) {
    if (strcmp(word, "local") == 0) {
      lcm_pm_local_setup_ops(&LCM_PM_ops);
    } else if (strcmp(word, "pmi1") == 0) {
      lcm_pm_pmi1_setup_ops(&LCM_PM_ops);
    } else if (strcmp(word, "pmi2") == 0) {
      lcm_pm_pmi2_setup_ops(&LCM_PM_ops);
    } else if (strcmp(word, "pmix") == 0) {
#ifdef LCI_PM_BACKEND_ENABLE_PMIX
      lcm_pm_pmix_setup_ops(&LCM_PM_ops);
#else
      if (enable_log)
        fprintf(stderr, "LCI is not compiled with the %s backend. Skip.\n",
                word);
      continue;
#endif
    } else if (strcmp(word, "mpi") == 0) {
#ifdef LCI_PM_BACKEND_ENABLE_MPI
      lcm_pm_mpi_setup_ops(&LCM_PM_ops);
#else
      if (enable_log)
        fprintf(stderr, "LCI is not compiled with the %s backend. Skip.\n",
                word);
      continue;
#endif
    } else
      LCM_Assert(
          false,
          "Unknown env LCM_PM_BACKEND (%s against local|pmi1|pmi2|pmix|mpi).\n",
          word);
    if (LCM_PM_ops.check_availability()) {
      if (enable_log) fprintf(stderr, "Use %s as the PMI backend.\n", word);
      found_valid_backend = true;
      break;
    } else {
      if (enable_log)
        fprintf(stderr, "The PMI backend %s is not available.\n", word);
      continue;
    }
  }
  free(str);
  LCM_Assert(found_valid_backend,
             "Tried [%s]. Did not find valid PMI backend! Give up!\n", p);
  LCM_PM_ops.initialize();
}

int lcm_pm_initialized() { return LCM_PM_ops.is_initialized(); }
int lcm_pm_get_rank() { return LCM_PM_ops.get_rank(); }
int lcm_pm_get_size() { return LCM_PM_ops.get_size(); }
void lcm_pm_publish(char* key, char* value) { LCM_PM_ops.publish(key, value); }
void lcm_pm_getname(int rank, char* key, char* value)
{
  LCM_PM_ops.getname(rank, key, value);
}
void lcm_pm_barrier() { LCM_PM_ops.barrier(); }
void lcm_pm_finalize() { LCM_PM_ops.finalize(); }