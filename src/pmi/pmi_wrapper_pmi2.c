#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.h"
#include "pmi2.h"

int lcm_pm_pmi2_check_availability()
{
  char* p = getenv("PMI_RANK");
  if (p)
    return true;
  else
    return false;
}

void lcm_pm_pmi2_initialize()
{
  int spawned, appnum, rank, size;
  PMI2_Init(&spawned, &size, &rank, &appnum);
}

int lcm_pm_pmi2_initialized() { return PMI2_Initialized(); }
int lcm_pm_pmi2_get_rank()
{
  int rank;
  PMI2_Job_GetRank(&rank);
  return rank;
}

int lcm_pm_pmi2_get_size()
{
  int size;
  PMI2_Info_GetSize(&size);
  return size;
}

void lcm_pm_pmi2_publish(char* key, char* value) { PMI2_KVS_Put(key, value); }

void lcm_pm_pmi2_getname(int rank, char* key, char* value)
{
  int vallen;
  PMI2_KVS_Get(NULL, rank /* PMI2_ID_NULL */, key, value, LCM_PMI_STRING_LIMIT,
               &vallen);
}

void lcm_pm_pmi2_barrier()
{
  // WARNING: Switching to PMI2 breaks this barrier
  PMI2_KVS_Fence();
}

void lcm_pm_pmi2_finalize() { PMI2_Finalize(); }

void lcm_pm_pmi2_setup_ops(struct LCM_PM_ops_t* ops)
{
  ops->check_availability = lcm_pm_pmi2_check_availability;
  ops->initialize = lcm_pm_pmi2_initialize;
  ops->is_initialized = lcm_pm_pmi2_initialized;
  ops->get_rank = lcm_pm_pmi2_get_rank;
  ops->get_size = lcm_pm_pmi2_get_size;
  ops->publish = lcm_pm_pmi2_publish;
  ops->getname = lcm_pm_pmi2_getname;
  ops->barrier = lcm_pm_pmi2_barrier;
  ops->finalize = lcm_pm_pmi2_finalize;
}