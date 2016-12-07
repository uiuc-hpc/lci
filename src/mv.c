// TODO(danghvu): Ugly hack to make thread-local storage faster.

#include "mpi.h"
#include "mv.h"
#include "mv-inl.h"
#include <stdint.h>
#include "pool.h"

__thread int8_t
tls_pool_struct[MAX_LOCAL_POOL] = {-1, -1, -1, -1, -1, -1, -1, -1};

int mv_pool_nkey = 0;

uint8_t mv_am_register(mv_engine* mv, mv_am_func_t f)
{
  mv->am_table[mv->am_table_size ++] = f;
  MPI_Barrier(MPI_COMM_WORLD);
  return mv->am_table_size - 1;
}


void* mv_heap_ptr(mv_engine* mv)
{
  return mv_server_heap_ptr(mv->server);
}


