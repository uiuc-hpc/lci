#include "lc.h"

#include "coll/common.h"
#include "coll/ialreduce.h"
#include "coll/ibarrier.h"
#include "coll/ibcast.h"

void lc_ialreduce(const void *sbuf, void *rbuf, size_t count, ompi_op_t op, lc_ep ep, lc_colreq* req)
{
  iompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, op, ep, req);
}

void lc_ibarrier(lc_ep ep, lc_colreq* req)
{
  iompi_coll_base_barrier_intra_bruck(ep, req);
}

void lc_ibcast(void *buf, size_t count, int root, lc_ep ep, lc_colreq* req)
{
  iompi_coll_base_bcast_intra_basic_linear(buf, count, root, ep, req);
}

void lc_alreduce(const void *sbuf, void *rbuf, size_t count, ompi_op_t op, lc_ep ep)
{
  lc_colreq req;
  iompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, op, ep, &req);
  while (!req.flag)
    lc_col_progress(&req);
}

void lc_barrier(lc_ep ep)
{
  lc_colreq req;
  iompi_coll_base_barrier_intra_bruck(ep, &req);
  while (!req.flag)
    lc_col_progress(&req);
}

void lc_bcast(void *buf, size_t count, int root, lc_ep ep)
{
  lc_colreq req;
  iompi_coll_base_bcast_intra_basic_linear(buf, count, root, ep, &req);
  while (!req.flag)
    lc_col_progress(&req);
}
