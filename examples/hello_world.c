#include "lci.h"
#include <stdio.h>

int main(int argc, char** args)
{
  // Call `LCI_initialize` to initialize the runtime
  LCI_initialize();
  // After initialization, `LCI_RANK` and `LCI_NUM_PROCESSES` are available to
  // use.
  // LCI_RANK is the id of the current process
  // LCI_NUM_PROCESSES is the total number of the processes in the current job.
  printf("%d / %d OK\n", LCI_RANK, LCI_NUM_PROCESSES);
  // call `LCI_finalize` to finalize the runtime
  LCI_finalize();
}
