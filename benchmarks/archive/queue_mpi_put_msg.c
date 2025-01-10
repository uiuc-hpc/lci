#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdlib.h>

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

  MPI_Win win;
  MPI_Group comm_group, group;
  MPI_Comm_group(MPI_COMM_WORLD, &comm_group);
  int destrank = 1 - rank;
  MPI_Group_incl(comm_group, 1, &destrank, &group);
  void* buffer;

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
      buffer = malloc(len);
      memset(buffer, 'A', len);
      MPI_Info info;
      MPI_Info_create(&info);
      MPI_Info_set(info, "no_locks", "true");
      MPI_Info_set(info, "same_disp_unit", "true");

      MPI_Win_create(buffer, len, 1, info, MPI_COMM_WORLD, &win);
      MPI_Info_free(&info);

      if (rank == 0) {
        double t1;
        int rank, tag, size;
        for (int i = 0; i < skip + total; i++) {
          MPI_Win_start(group, 0, win);
          if (i == skip) t1 = MPI_Wtime();
          for (int j = 0; j < WINDOWS; j++)
            MPI_Put(buffer, len, MPI_BYTE, 1, 0, 0, MPI_BYTE, win);
          MPI_Win_complete(win);
          MPI_Win_post(group, 0, win);
          MPI_Win_wait(win);
        }
        t1 = MPI_Wtime() - t1;
        printf("%d \t %.5f %.5f \n", WINDOWS, total * (WINDOWS + 1) / t1,
               total * (WINDOWS + 1) * len / 1e6 / t1);
      } else {
        for (int i = 0; i < skip + total; i++) {
          MPI_Win_post(group, 0, win);
          MPI_Win_wait(win);
          MPI_Win_start(group, 0, win);
          MPI_Put(buffer, len, MPI_BYTE, 0, 0, 0, MPI_BYTE, win);
          MPI_Win_complete(win);
        }
      }
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Win_free(&win);
      free(buffer);
    }
  MPI_Finalize();
  return 0;
}
