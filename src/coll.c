#include "lc.h"

#include "coll/common.h"
#include "coll/ialreduce.h"
#include "coll/ibarrier.h"

void lc_ialreduce(const void *sbuf, void *rbuf, size_t count, ompi_op_t op, lc_ep ep, lc_colreq* req)
{
  iompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, op, ep, req);
}

void lc_ibarrier(lc_ep ep, lc_colreq* req)
{
  iompi_coll_base_barrier_intra_bruck(ep, req);
}
