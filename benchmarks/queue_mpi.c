#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mpi.h"

#include "comm_exp.h"

int WINDOWS = 1;

int main(int argc, char** args)
{
  MPI_Init(&argc, &args);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int total, skip;

  if (argc > 2)
    WINDOWS = atoi(args[1]);

  for (size_t len = 1; len <= (1 << 22); len <<= 1) {
    if (len > 8192) {
      skip = SKIP_LARGE;
      total = TOTAL_LARGE;
    } else {
      skip = SKIP;
      total = TOTAL;
    }
    void* buffer = malloc(len);
    memset(buffer, 'A', len);
    if (rank == 0) {
      double t1;
      void *recv = malloc(len);
      int rank, tag, size;
      for (int i = 0; i < skip + total; i++) {
        if (i == skip) t1 = MPI_Wtime();
        for (int j = 0 ; j < WINDOWS; j++)
          // send
          MPI_Send(buffer, len, MPI_BYTE, 1, 0, MPI_COMM_WORLD);

        // recv.
        MPI_Status stat;
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
        MPI_Get_count(&stat, MPI_BYTE, &size);
        rank = stat.MPI_SOURCE;
        tag = stat.MPI_TAG;
        MPI_Recv(recv, size, MPI_BYTE, rank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (i == 0) {
          assert(rank == 1);
          assert(tag == i);
          assert(size == len);
          for (int j = 0; j < size; j++) {
            assert(((char*)recv)[j] == 'A');
          }
        }
      }
      free(recv);
      printf("%d \t %.5f\n", len, (MPI_Wtime() - t1)/total / (WINDOWS+1) * 1e6);
    } else {
      void* recv = malloc(len);
      for (int i = 0; i < skip + total; i++) {
        for (int j = 0 ; j < WINDOWS; j++) {
          int rank, tag, size;
          MPI_Status stat;
          MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
          MPI_Get_count(&stat, MPI_BYTE, &size);
          rank = stat.MPI_SOURCE;
          tag = stat.MPI_TAG;
          MPI_Recv(recv, size, MPI_BYTE, rank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // send
        MPI_Send(buffer, len, MPI_BYTE, 0, i, MPI_COMM_WORLD);
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    free(buffer);
  }
  MPI_Finalize();
  return 0;
}
