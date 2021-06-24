#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.h"
#ifdef LCI_USE_PMI1
#include "pmi.h"
#endif
#ifdef LCI_USE_PMI2
#include "pmi2.h"
#endif

static char lcg_name[256];

void lc_pm_master_init(int* size, int* rank)
{
  int spawned;
#ifdef LCI_USE_PMI1
  PMI_Init(&spawned, size, rank);
  PMI_KVS_Get_my_name(lcg_name, 255);
#endif
#ifdef LCI_USE_PMI2
  int appnum;
  PMI2_Init(&spawned, size, rank, &appnum);
  PMI2_Job_GetId(lcg_name, 255);
#endif
#ifdef LCI_USE_FILE_AS_PM
  char* val = getenv("");
  if (val != NULL) {
    return atoi(val);
  } else {
    return def;
  }
#endif
}

void lc_pm_publish_key(char* key, char* value)
{
#ifdef LCI_USE_PMI1
  PMI_KVS_Put(lcg_name, key, value);
#endif
#ifdef LCI_USE_PMI2
  PMI2_KVS_Put(key, value);
#endif
}

void lc_pm_getname_key(char* key, char* value)
{
#ifdef LCI_USE_PMI1
  PMI_KVS_Get(lcg_name, key, value, 255);
#endif
#ifdef LCI_USE_PMI2
  int vallen;
  PMI2_KVS_Get(lcg_name, PMI2_ID_NULL, key, value, 255, &vallen);
#endif
}

void lc_pm_publish(int rank, int gid, char* value)
{
  char key[256];
  sprintf(key, "_LC_KEY_%d_%d", rank, gid);
  lc_pm_publish_key(key, value);
}

void lc_pm_getname(int rank, int gid, char* value)
{
  char key[256];
  sprintf(key, "_LC_KEY_%d_%d", rank, gid);
  lc_pm_getname_key(key, value);
}

void lc_pm_barrier() {
#ifdef LCI_USE_PMI1
  PMI_Barrier();
#endif
#ifdef LCI_USE_PMI2
  // WARNING: Switching to PMI2 breaks this barrier
  PMI2_KVS_Fence();
#endif
}

void lc_pm_finalize() {
#ifdef LCI_USE_PMI1
  PMI_Finalize();
#endif
#ifdef LCI_USE_PMI2
  PMI2_Finalize();
#endif
}