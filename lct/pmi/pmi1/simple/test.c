#include "pmi.h"
#include <stdio.h>

int main(int argc, char** args)
{
  int spawned;
  int size;
  int rank;
  PMI_Init(&spawned);
  PMI_Get_rank(&rank);
  PMI_Get_size(&size);
  printf("%d %d %d\n", spawned, size, rank);

  char key[256];
  char val[256];
  char name[256];
  PMI_KVS_Get_my_name(name, 255);

  snprintf(key, 256, "_LC_KEY_%d", rank);
  snprintf(val, 256, "_LC_VAL_%d", rank);
  PMI_KVS_Put(name, key, val);
  PMI_Barrier();

  int i;
  for (i = 1; i < size; i++) {
    int target = (rank + i) % size;
    snprintf(key, 256, "_LC_KEY_%d", target);
    PMI_KVS_Get(name, key, val, 255);
    printf("%d >> %s: %s\n", rank, key, val);
  }

  PMI_Finalize();
}
