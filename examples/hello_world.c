#include "lci.h"
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

int main(int argc, char** args)
{
  char hostname[HOST_NAME_MAX + 1];
  gethostname(hostname, HOST_NAME_MAX + 1);
  // Call `LCI_initialize` to initialize the runtime
  LCI_initialize();
  // After initialization, `LCI_RANK` and `LCI_NUM_PROCESSES` are available to
  // use.
  // LCI_RANK is the id of the current process
  // LCI_NUM_PROCESSES is the total number of the processes in the current job.
  printf("%s: %d / %d OK\n", hostname, LCI_RANK, LCI_NUM_PROCESSES);
  // Call `LCI_finalize` to finalize the runtime
  LCI_finalize();
}
