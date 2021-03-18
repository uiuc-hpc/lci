#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  LCI_open();
  printf("%d / %d OK\n", LCI_RANK, LCI_NUM_PROCESSES);
  LCI_close();
  return 0;
}
