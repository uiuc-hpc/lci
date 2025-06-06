#include <stdio.h>
#include "pmi2.h"

int main(int argc, char** args)
{
  int spawned;
  int size;
  int rank;
  int appnum;
  PMI2_Init(&spawned, &size, &rank, &appnum);
  printf("%d %d %d %d\n", spawned, size, rank, appnum);

  char key[256];
  char val[256];

  snprintf(key, 256, "_LC_KEY_%d", rank);
  snprintf(val, 256, "_LC_VAL_%d", rank);
  PMI2_KVS_Put(key, val);
  PMI2_KVS_Fence();

  int i;
  for (i = 1; i < size; i++) {
    int target = (rank + i) % size;
    snprintf(key, 256, "_LC_KEY_%d", target);
    int vallen;
    PMI2_KVS_Get(NULL, target, key, val, 255, &vallen);
    printf("%d >> %s: %s\n", rank, key, val);
  }

  PMI2_Finalize();
}
