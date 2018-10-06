#include "lc.h"

#include "coll/common.h"
#include "coll/algather.h"
#include "coll/alreduce.h"
#include "coll/ialreduce.h"
#include "coll/barrier.h"
#include "coll/ibarrier.h"

void lc_algather(void* sbuf, size_t scount, void* rbuf, size_t rcount, lc_ep ep)
{
  ompi_coll_tuned_allgather_intra_bruck(sbuf, scount, rbuf, rcount, ep);
}

void lc_alreduce(const void *sbuf, void *rbuf, size_t count, ompi_op_t op, lc_ep ep)
{
  ompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, op, ep);
}

void lc_ialreduce(const void *sbuf, void *rbuf, size_t count, ompi_op_t op, lc_ep ep, lc_colreq* req)
{
  iompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, op, ep, req);
}

void lc_barrier(lc_ep ep)
{
  ompi_coll_base_barrier_intra_bruck(ep);
}

void lc_ibarrier(lc_ep ep, lc_colreq* req)
{
  iompi_coll_base_barrier_intra_bruck(ep, req);
}
