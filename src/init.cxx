#include "mv.h"
#include "mv-inl.h"

void mv_open(int *argc, char*** args, mv_engine** ret)
{
  mv_struct *mv = new mv_struct();

  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);

  int provided;
  MPI_Init_thread(argc, args, MPI_THREAD_MULTIPLE, &provided);
  assert(MPI_THREAD_MULTIPLE == provided);

  mv_hash_init(&mv->tbl);
  mv_progress_init();

  mv_pp_init(&mv->pkpool);

  mv_server_init(&(mv->server), mv, mv->pkpool, mv->me, mv->size);
  mv_server_serve(mv->server);
  MPI_Barrier(MPI_COMM_WORLD);

  *ret = mv;
}

void mv_close(mv_engine* mv)
{
  MPI_Barrier(MPI_COMM_WORLD);
  mv_server_finalize(mv->server);
  mv_pp_destroy(mv->pkpool);
  MPI_Finalize();
}
