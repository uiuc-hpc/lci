#ifndef LC_IALREDUCE_H
#define LC_IALREDUCE_H

#include <stddef.h>

static int MCA_COLL_BASE_TAG_IALLREDUCE = 1340;
static inline int opal_next_poweroftwo(int value);

int iompi_coll_base_allreduce_intra_recursivedoubling(
    const void *sbuf, void *rbuf, size_t count,
    ompi_op_t op, lc_ep comm, lc_colreq* req)
{
  int rank, size, adjsize, remote, distance;
  int newrank, newremote, extra_ranks;
  char *tmpsend = NULL, *tmprecv = NULL, *inplacebuf_free = NULL, *inplacebuf;
  ptrdiff_t gap = 0;

  lc_get_proc_num(&rank);
  lc_get_num_proc(&size);

  /* Special case for size == 1 */
  if (1 == size) {
    memmove(rbuf, sbuf, count);
    req->flag = 1;
    return 0;
  }

  lc_colreq_init(req);
  req->op = op;

  /* Allocate and initialize temporary send buffer */
  inplacebuf_free = (char*) malloc(count);
  inplacebuf = inplacebuf_free - gap;

  if (LC_COL_IN_PLACE == sbuf) {
    memmove(inplacebuf, (char*)rbuf, count);
  } else {
    memmove(inplacebuf, (char*)sbuf, count);
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
  if (rank <  (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      lc_col_send(tmpsend, count, (rank + 1),
                  MCA_COLL_BASE_TAG_IALLREDUCE, comm, req);
      newrank = -1;
    } else {
      lc_col_recv(tmprecv, count, (rank - 1),
                  MCA_COLL_BASE_TAG_IALLREDUCE, comm, req);
      /* tmpsend = tmprecv (op) tmpsend */
      lc_col_op(tmpsend, tmprecv, count, req);
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
    // lc_col_send(tmpsend, count, remote, MCA_COLL_BASE_TAG_IALLREDUCE, comm, req);
    // lc_col_recv(tmprecv, count, remote, MCA_COLL_BASE_TAG_IALLREDUCE, comm, req);
    lc_col_sendrecv(tmpsend, tmprecv, count, remote, MCA_COLL_BASE_TAG_IALLREDUCE, comm, req);

    /* Apply operation */
    lc_col_op(tmpsend, tmprecv, count, req);
  }

  /* Handle non-power-of-two case:
   *        - Odd ranks less than 2 * extra_ranks send result from tmpsend to
   *        (rank - 1)
   *        - Even ranks less than 2 * extra_ranks receive result from (rank + 1)
   *        */
  if (rank < (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      lc_col_recv(rbuf, count, (rank + 1),
                  MCA_COLL_BASE_TAG_IALLREDUCE, comm, req);
      tmpsend = (char*)rbuf;
    } else {
      lc_col_send(tmpsend, count, (rank - 1),
                  MCA_COLL_BASE_TAG_IALLREDUCE, comm, req);
    }
  }

  /* Ensure that the final result is in rbuf */
  if (tmpsend != rbuf) {
    lc_col_memmove((char*)rbuf, tmpsend, count, req);
  }

  if (NULL != inplacebuf_free) lc_col_free(inplacebuf_free, req);
  lc_col_progress(req);
  return 0;
}

#endif
