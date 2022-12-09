#ifndef LC_IBARRIER_H_
#define LC_IBARRIER_H_

static int MCA_COLL_BASE_TAG_IBARRIER = 1341;

int iompi_coll_base_barrier_intra_bruck(lc_ep comm, lc_colreq* req)
{
  int rank, size, distance, to, from;
  lc_get_num_proc(&size);
  lc_get_proc_num(&rank);

  lc_colreq_init(req);

  /* exchange data with rank-2^k and rank+2^k */
  for (distance = 1; distance < size; distance <<= 1) {
    from = (rank + size - distance) % size;
    to = (rank + distance) % size;

    /* send message to lower ranked node */
    lc_col_send(&(req->empty), 1, to, MCA_COLL_BASE_TAG_IBARRIER, comm, req);

    lc_col_recv(&(req->empty), 1, from, MCA_COLL_BASE_TAG_IBARRIER, comm, req);
  }

  lc_col_progress(req);
  return 0;
}

#endif
