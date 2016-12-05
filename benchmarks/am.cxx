#include "helper.h"
#include "mpiv.h"

int8_t fid;
uint64_t rmt_dma_buf;
volatile int done = 0;
extern mv_engine* mv_hdl;

void test(void* buf, uint32_t size)
{
  assert(size == 8);
  rmt_dma_buf = *((uint64_t*)buf);
  void* dma_src = MPIV_Alloc(64);
  memset(dma_src, 'A', 64);
  int me;
  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  mv_put(mv_hdl, 1 - me, (void*)rmt_dma_buf, dma_src, 64, 1337);
  done = 1;
}

int main(int argc, char** args)
{
  MPIV_Init(&argc, &args);
  fid = mv_am_register(mv_hdl, (mv_am_func_t)test);
  MPIV_Start_worker(1);
  MPIV_Finalize();
}

void main_task(intptr_t)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  void* dma_buf = MPIV_Alloc(64);
  memset(dma_buf, 'B', 64);
  uint64_t addr = (uint64_t)dma_buf;
  mv_am_eager(mv_hdl, 1 - rank, &addr, 8, fid);
  while (!done) {
  }
  for (int i = 0; i < 64; i++)
    while (((volatile char*)dma_buf)[i] == 'B') {
    };
}
