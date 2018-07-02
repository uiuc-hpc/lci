#include <stdlib.h>
#include <stdio.h>


#include "pmi.h"
#include "pm.h"

void lc_pm_master_init(int* size, int* rank, char* name)
{
  char key[256];
  char value[256];
  int spawned;
  PMI_Init(&spawned, size, rank);
  PMI_KVS_Get_my_name(name, 255);
}

void lc_pm_publish(int prank, int erank, char* name, char* value)
{
  char key[256];
  sprintf(key, "_LC_KEY_%d_%d", prank, erank);
  PMI_KVS_Put(name, key, value);
  PMI_Barrier();
}

void lc_pm_getname(int prank, int erank, char* name, char* value)
{
  char key[256];
  sprintf(key, "_LC_KEY_%d_%d", prank, erank);
  PMI_KVS_Get(name, key, value, 255);
}
