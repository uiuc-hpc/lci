#include "lc.h"

#include "coll/algather.h"
#include "coll/alreduce.h"
#include "coll/barrier.h"

void lc_algather(void* sbuf, size_t scount, void* rbuf, size_t rcount, lch* mv)
{
  ompi_coll_tuned_allgather_intra_bruck(sbuf, scount, rbuf, rcount, mv);
}

void lc_alreduce(const void *sbuf, void *rbuf, size_t count, ompi_op_t op, lch* mv)
{
  ompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, op, mv);
}

void lc_barrier(lch* mv)
{
  ompi_coll_base_barrier_intra_bruck(mv);
}
