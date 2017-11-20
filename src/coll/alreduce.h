#ifndef LC_ALREDUCE_H
#define LC_ALREDUCE_H

#include <string.h>
#include <assert.h>

static lc_info MCA_COLL_BASE_TAG_ALLREDUCE = {LC_SYNC_NULL, LC_SYNC_NULL, {.tag = 1339}};

static inline int opal_next_poweroftwo(int value)
{
  int power2;

  if (0 == value) {
    return 1;
  }
  power2 = 1 << (8 * sizeof (int) - __builtin_clz(value));

  return power2;
}

int ompi_coll_base_allreduce_intra_recursivedoubling(
    const void *sbuf, void *rbuf, size_t count,
    ompi_op_t op, lch* comm)
{
  int rank, size, adjsize, remote, distance;
  int newrank, newremote, extra_ranks;
  char *tmpsend = NULL, *tmprecv = NULL, *tmpswap = NULL, *inplacebuf_free = NULL, *inplacebuf;
  ptrdiff_t gap = 0;

  size = lc_size(comm);
  rank = lc_id(comm);

  /* Special case for size == 1 */
  if (1 == size) {
    memcpy(rbuf, sbuf, count);
    return 0;
  }

  /* Allocate and initialize temporary send buffer */
  inplacebuf_free = (char*) malloc(count);
  if (!inplacebuf_free)
    fprintf(stderr, "%d> Unable to allocate ??? %ld\n", rank, count);
  assert(inplacebuf_free);
  inplacebuf = inplacebuf_free - gap;

  if (LC_COL_IN_PLACE == sbuf) {
    memcpy(inplacebuf, (char*)rbuf, count);
  } else {
    memcpy(inplacebuf, (char*)sbuf, count);
  }

  tmpsend = (char*) inplacebuf;
  tmprecv = (char*) rbuf;

  /* Determine nearest power of two less than or equal to size */
  adjsize = opal_next_poweroftwo (size);
  adjsize >>= 1;

  /* Handle non-power-of-two case:
   *        - Even ranks less than 2 * extra_ranks send their data to (rank + 1), and
   *        sets new rank to -1.
   *        - Odd ranks less than 2 * extra_ranks receive data from (rank - 1),
   *        apply appropriate operation, and set new rank to rank/2
   *        - Everyone else sets rank to rank - extra_ranks
   *        */
  extra_ranks = size - adjsize;
  lc_req sreq, rreq;
  if (rank <  (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      LC_SAFE(lc_send_tag(comm, tmpsend, count, (rank + 1),
            &MCA_COLL_BASE_TAG_ALLREDUCE, &sreq));
      lc_wait(&sreq);
      newrank = -1;
    } else {
      lc_recv_tag(comm, tmprecv, count, (rank - 1),
            &MCA_COLL_BASE_TAG_ALLREDUCE, &rreq);
      lc_wait(&rreq);
      /* tmpsend = tmprecv (op) tmpsend */
      op(tmpsend, tmprecv, count);
      newrank = rank >> 1;
    }
  } else {
    newrank = rank - extra_ranks;
  }

  /* Communication/Computation loop
   * - Exchange message with remote node.
   * - Perform appropriate operation taking in account order of operations:
   * result = value (op) result
   * */
  for (distance = 0x1; distance < adjsize; distance <<=1) {
    if (newrank < 0) break;
    /* Determine remote node */
    newremote = newrank ^ distance;
    remote = (newremote < extra_ranks)?
      (newremote * 2 + 1):(newremote + extra_ranks);

    /* Exchange the data */
    LC_SAFE(lc_send_tag(comm, tmpsend, count, remote, &MCA_COLL_BASE_TAG_ALLREDUCE, &sreq));
    lc_recv_tag(comm, tmprecv, count, remote, &MCA_COLL_BASE_TAG_ALLREDUCE, &rreq);
    lc_wait(&sreq);
    lc_wait(&rreq);

    /* Apply operation */
    if (rank < remote) {
      /* tmprecv = tmpsend (op) tmprecv */
      op(tmprecv, tmpsend, count);
      tmpswap = tmprecv;
      tmprecv = tmpsend;
      tmpsend = tmpswap;
    } else {
      /* tmpsend = tmprecv (op) tmpsend */
      op(tmpsend, tmprecv, count);
    }
  }

  /* Handle non-power-of-two case:
   *        - Odd ranks less than 2 * extra_ranks send result from tmpsend to
   *        (rank - 1)
   *        - Even ranks less than 2 * extra_ranks receive result from (rank + 1)
   *        */
  if (rank < (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      lc_recv_tag(comm, rbuf, count, (rank + 1),
          &MCA_COLL_BASE_TAG_ALLREDUCE, &rreq);
      lc_wait(&rreq);
      tmpsend = (char*)rbuf;
    } else {
      LC_SAFE(lc_send_tag(comm, tmpsend, count, (rank - 1),
            &MCA_COLL_BASE_TAG_ALLREDUCE, &sreq));
      lc_wait(&sreq);
    }
  }

  /* Ensure that the final result is in rbuf */
  if (tmpsend != rbuf) {
    memcpy((char*)rbuf, tmpsend, count);
  }

  if (NULL != inplacebuf_free) free(inplacebuf_free);
  return 0;
}

#endif
