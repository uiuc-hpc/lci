#ifndef LC_BARRIER_H_
#define LC_BARRIER_H_

static int round = 0;

static lc_info MCA_COLL_BASE_TAG_BARRIER = {LC_SYNC_NULL, LC_SYNC_NULL, {.tag = 1337}};

int ompi_coll_base_barrier_intra_bruck(lch* comm)
{
  int rank, size, distance, to, from;

  rank = lc_id(comm);
  size = lc_size(comm);
  int tempsend, temprecv;
  lc_req sreq, rreq;

  /* exchange data with rank-2^k and rank+2^k */
  for (distance = 1; distance < size; distance <<= 1) {
    from = (rank + size - distance) % size;
    to   = (rank + distance) % size;

    /* send message to lower ranked node */
    LC_SAFE(lc_send_tag(comm, &tempsend, 0, to,
        &MCA_COLL_BASE_TAG_BARRIER, &sreq));

    lc_recv_tag(comm, &temprecv, 0, from,
        &MCA_COLL_BASE_TAG_BARRIER, &rreq);

    lc_wait(&sreq);
    lc_wait(&rreq);
  }

  round++;

  return 0;
}

#endif
