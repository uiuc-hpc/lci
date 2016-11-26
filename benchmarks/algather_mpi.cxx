#include "comm_exp.h"
#include "mpi.h"
#include <iostream>

int main(int argc, char** args)
{
  MPI_Init(&argc, &args);
  int rank, commsize;
  double t = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &commsize);

  int size = 1024;
  void* recvbuf = malloc(size * commsize);
  for (int i = 0; i < TOTAL_LARGE + SKIP_LARGE; i++) {
    double t1 = wtime();
    MPI_Allgather(MPI_IN_PLACE, size, MPI_CHAR, recvbuf, size, MPI_CHAR,
                  MPI_COMM_WORLD);
    t1 = wtime() - t1;
    if (i >= SKIP_LARGE) t += t1;
    MPI_Barrier(MPI_COMM_WORLD);
  }
  if (rank == 0) std::cout << "Time: " << 1e6 * t / TOTAL_LARGE << std::endl;
  MPI_Finalize();
  return 0;
}
