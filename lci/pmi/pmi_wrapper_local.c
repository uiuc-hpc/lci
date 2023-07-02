#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pmi_wrapper.h"
#include "pmii_archive.h"

static int initialized = 0;
static Archive_t l_archive;

int lcm_pm_local_check_availability()
{
#ifndef LCI_PM_BACKEND_ENABLE_PMIX
  if (getenv("PMIX_RANK"))
    LCI_Warn(
        "LCI detects the PMIx environment. However, the LCI PMIx support is "
        "not enabled. LCI assumes the number of processes of this job is 1. If "
        "you intended to run more than one processes, please do one of the "
        "followings:\n"
        "\t(a) set up the PMI2 environment by `srun --mpi=pmi2 "
        "[application]`.\n"
        "\t(b) set PMIX_ROOT when configuring LCI to enable the PMIx support.\n"
        "\t(c) set MPI_ROOT when configuring LCI to enable the MPI support "
        "(last resort).\n");
#endif
  return true;
}

void lcm_pm_local_initialize()
{
  archive_init(&l_archive);
  initialized = 1;
}

int lcm_pm_local_initialized() { return initialized; }
int lcm_pm_local_get_rank() { return 0; }

int lcm_pm_local_get_size() { return 1; }

void lcm_pm_local_publish(char* key, char* value)
{
  archive_push(&l_archive, key, value);
}

void lcm_pm_local_getname(int rank, char* key, char* value)
{
  char* ret = archive_search(&l_archive, key);
  strcpy(value, ret);
}

void lcm_pm_local_barrier() {}

void lcm_pm_local_finalize()
{
  archive_fina(&l_archive);
  initialized = 0;
}

void lcm_pm_local_setup_ops(struct LCM_PM_ops_t* ops)
{
  ops->check_availability = lcm_pm_local_check_availability;
  ops->initialize = lcm_pm_local_initialize;
  ops->is_initialized = lcm_pm_local_initialized;
  ops->get_rank = lcm_pm_local_get_rank;
  ops->get_size = lcm_pm_local_get_size;
  ops->publish = lcm_pm_local_publish;
  ops->getname = lcm_pm_local_getname;
  ops->barrier = lcm_pm_local_barrier;
  ops->finalize = lcm_pm_local_finalize;
}