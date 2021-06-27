#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.h"
#include "pmi.h"

void lcm_pm_initialize()
{
  int spawned, rank_me, nranks;
  PMI_Init(&spawned, &nranks, &rank_me);
}

int lcm_pm_initialized() {
  int initialized;
  PMI_Initialized(&initialized);
  return initialized;
}
int lcm_pm_get_rank() {
  int rank;
  PMI_Get_rank(&rank);
  return rank;
}

int lcm_pm_get_size() {
  int size;
  PMI_Get_universe_size(&size);
  return size;
}

void lcm_pm_publish(char* key, char* value)
{
  char lcg_name[256];
  PMI_KVS_Get_my_name(lcg_name, 255);
  PMI_KVS_Put(lcg_name, key, value);
}

void lcm_pm_getname(char* key, char* value)
{
  char lcg_name[256];
  PMI_KVS_Get_my_name(lcg_name, 255);
  PMI_KVS_Get(lcg_name, key, value, 255);
}

void lcm_pm_barrier() {
  PMI_Barrier();
}

void lcm_pm_finalize() {
  PMI_Finalize();
}