#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mpi.h"

#include "comm_exp.h"

int WINDOWS = 1;
int SIZE = 64;

int main(int argc, char** args)
{
  MPI_Init(&argc, &args);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int total, skip;

  if (argc > 1) SIZE = atoi(args[1]);

  for (WINDOWS = 1; WINDOWS <= 1024; WINDOWS <<= 1)
    for (size_t len = SIZE; len <= SIZE; len <<= 1) {
      MPI_Request req[WINDOWS];
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
        void* recv = malloc(len);
        int rank, tag, size;
        for (int i = 0; i < skip + total; i++) {
          if (i == skip) t1 = MPI_Wtime();
          for (int j = 0; j < WINDOWS; j++)
            // send
            MPI_Isend(buffer, len, MPI_BYTE, 1, 0, MPI_COMM_WORLD, &req[j]);
          MPI_Waitall(WINDOWS, req, MPI_STATUSES_IGNORE);

          // recv.
          MPI_Recv(recv, len, MPI_BYTE, 1, 0, MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);

          if (i == -1) {
            assert(rank == 1);
            assert(tag == i);
            assert(size == len);
            for (int j = 0; j < size; j++) {
              assert(((char*)recv)[j] == 'A');
            }
          }
        }
        free(recv);
        t1 = MPI_Wtime() - t1;
        printf("%d \t %.5f %.5f \n", WINDOWS, total * (WINDOWS + 1) / t1,
               total * (WINDOWS + 1) * len / 1e6 / t1);
      } else {
        void* recv = malloc(len);
        MPI_Request req[WINDOWS];
        for (int i = 0; i < skip + total; i++) {
          for (int j = 0; j < WINDOWS; j++) {
            MPI_Irecv(recv, len, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &req[j]);
          }
          MPI_Waitall(WINDOWS, req, MPI_STATUSES_IGNORE);
          MPI_Send(buffer, len, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
        }
        free(recv);
      }
      MPI_Barrier(MPI_COMM_WORLD);
      free(buffer);
    }
  MPI_Finalize();
  return 0;
}
