#include "lc.h"
#include <mpi.h>

int main(int argc, char** args) {
  lch* mv;
  lc_open(&mv);

  printf("INit MPI\n");
  MPI_Init(&argc, &args);

  MPI_Finalize();

  lc_close(mv);
}
