#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  LCI_initialize(&argc, &args);
  printf("%d / %d OK\n", LCI_RANK, LCI_NUM_PROCESSES);
  LCI_finalize();
}
