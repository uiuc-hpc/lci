#include <mpi.h>
#include "mv.h"
#include "mv-inl.h"

void mv_open(int* argc, char*** args, size_t heap_size, mv_engine** ret)
{
  struct mv_struct* mv = malloc(sizeof(struct mv_struct));

  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);

  int provided;
  MPI_Init_thread(argc, args, MPI_THREAD_MULTIPLE, &provided);
  if (MPI_THREAD_MULTIPLE != provided) {
    printf("Need MPI_THREAD_MULTIPLE\n");
    exit(EXIT_FAILURE);
  }

  mv_hash_init(&mv->tbl);
  mv->am_table_size = 0;
  mv_progress_init(mv);
  mv_server_init(mv, heap_size, &mv->server);
  MPI_Barrier(MPI_COMM_WORLD);

  *ret = mv;
}

void mv_close(mv_engine* mv)
{
  MPI_Barrier(MPI_COMM_WORLD);
  mv_server_finalize(mv->server);
  MPI_Finalize();
}
