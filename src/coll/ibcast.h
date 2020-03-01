#ifndef LC_IBCAST_H
#define LC_IBCAST_H

#include <stddef.h>

static int MCA_COLL_BASE_TAG_IBCAST = 1342;

int iompi_coll_base_bcast_intra_basic_linear(
    void *buf, size_t count, int root, lc_ep comm, lc_colreq* req)
{
  int i, size, rank;

  lc_get_proc_num(&rank);
  lc_get_num_proc(&size);

  lc_colreq_init(req);

  if (rank != root) {
    /* Non-root receive the data. */
    lc_col_recv(buf, count, root, MCA_COLL_BASE_TAG_IBCAST, comm, req);
  } else {
    /* Root sends data to all others. */
    for (i = 0; i < size; i++) {
        if (i == rank)
            continue;
        lc_col_send(buf, count, i, MCA_COLL_BASE_TAG_IBCAST, comm, req);
    }
  }

  lc_col_progress(req);
  return 0;
}

#endif
