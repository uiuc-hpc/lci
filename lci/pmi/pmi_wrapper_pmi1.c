#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.h"
#include "pmi.h"

int lcm_pm_pmi1_check_availability()
{
  char* p = getenv("PMI_RANK");
  if (p)
    return true;
  else
    return false;
}

void lcm_pm_pmi1_initialize()
{
  int spawned;
  PMI_Init(&spawned);
}

int lcm_pm_pmi1_initialized()
{
  int initialized;
  PMI_Initialized(&initialized);
  return initialized;
}
int lcm_pm_pmi1_get_rank()
{
  int rank;
  PMI_Get_rank(&rank);
  return rank;
}

int lcm_pm_pmi1_get_size()
{
  int size;
  PMI_Get_size(&size);
  return size;
}

void lcm_pm_pmi1_publish(char* key, char* value)
{
  char lcg_name[LCM_PMI_STRING_LIMIT + 1];
  PMI_KVS_Get_my_name(lcg_name, LCM_PMI_STRING_LIMIT);
  PMI_KVS_Put(lcg_name, key, value);
}

void lcm_pm_pmi1_getname(int rank, char* key, char* value)
{
  char lcg_name[LCM_PMI_STRING_LIMIT + 1];
  PMI_KVS_Get_my_name(lcg_name, LCM_PMI_STRING_LIMIT);
  PMI_KVS_Get(lcg_name, key, value, LCM_PMI_STRING_LIMIT);
}

void lcm_pm_pmi1_barrier() { PMI_Barrier(); }

void lcm_pm_pmi1_finalize() { PMI_Finalize(); }

void lcm_pm_pmi1_setup_ops(struct LCM_PM_ops_t* ops)
{
  ops->check_availability = lcm_pm_pmi1_check_availability;
  ops->initialize = lcm_pm_pmi1_initialize;
  ops->is_initialized = lcm_pm_pmi1_initialized;
  ops->get_rank = lcm_pm_pmi1_get_rank;
  ops->get_size = lcm_pm_pmi1_get_size;
  ops->publish = lcm_pm_pmi1_publish;
  ops->getname = lcm_pm_pmi1_getname;
  ops->barrier = lcm_pm_pmi1_barrier;
  ops->finalize = lcm_pm_pmi1_finalize;
}