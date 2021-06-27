#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.h"
#include "pmi2.h"

void lcm_pm_initialize()
{
  int spawned, appnum, rank, size;
  PMI2_Init(&spawned, &size, &rank, &appnum);
}

int lcm_pm_initialized() {
  return PMI2_Initialized();
}
int lcm_pm_get_rank() {
  int rank;
  PMI2_Job_GetRank(&rank);
  return rank;
}

int lcm_pm_get_size() {
  int size;
  PMI2_Info_GetSize(&size);
  return size;
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