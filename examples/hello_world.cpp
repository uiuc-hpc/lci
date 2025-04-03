#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>

#include "lci.hpp"

int main(int argc, char** args)
{
  char hostname[HOST_NAME_MAX + 1];
  gethostname(hostname, HOST_NAME_MAX + 1);
  // Initialize the global default runtime.
  lci::g_runtime_init();
  // After at least one runtime is active, we can query the rank and nranks.
  // rank is the id of the current process
  // nranks is the total number of the processes in the current job.
  printf("%s: %d / %d OK\n", hostname, lci::get_rank(), lci::get_nranks());
  // Finalize the global default runtime
  lci::g_runtime_fina();
  return 0;
}
