#include <stdlib.h>
#include <stdio.h>

#include <mpi.h>

void _MPI_Init(int* argc, char*** args)
{
  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);
  int provided;
  MPI_Init_thread(argc, args, MPI_THREAD_MULTIPLE, &provided);
  if (MPI_THREAD_MULTIPLE != provided) {
    fprintf(stderr, "Need MPI_THREAD_MULTIPLE\n");
    exit(EXIT_FAILURE);
  }
}

