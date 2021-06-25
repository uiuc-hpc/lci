#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.h"
#include "pmi2.h"

void lcm_pm_initialize()
{
  int spawned, appnum, rank_me, nranks;
  PMI2_Init(&spawned, &nranks, &rank_me, &appnum);
}

int lcm_pm_initialized() {
  return PMI2_Initialized();
}
int lcm_pm_rank_me() {
  int rank_me;
  PMI2_Job_GetRank(&rank_me);
  return rank_me;
}

int lcm_pm_nranks() {
  int nranks;
  PMI2_Info_GetSize(&nranks);
  return nranks;
}

void lcm_pm_publish(char* key, char* value)
{
  PMI2_KVS_Put(key, value);
}

void lcm_pm_getname(char* key, char* value)
{
  int vallen;
  PMI2_KVS_Get(NULL, PMI2_ID_NULL, key, value, 255, &vallen);
}

void lcm_pm_barrier() {
  // WARNING: Switching to PMI2 breaks this barrier
  PMI2_KVS_Fence();
}

void lcm_pm_finalize() {
  PMI2_Finalize();
}