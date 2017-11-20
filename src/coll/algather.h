#ifndef LC_ALGATHER_H_
#define LC_ALGATHER_H_

#include <string.h>
#include <stdint.h>
#include <stddef.h>

static lc_info MCA_COLL_BASE_TAG_ALLGATHER = {LC_SYNC_NULL, LC_SYNC_NULL, {.tag = 1338}};

static inline int ompi_coll_tuned_allgather_intra_bruck(void* sbuf, size_t scount,
    void* rbuf, size_t rcount, lch* mv)
{
  int rank, size, sendto, recvfrom, distance, blockcount;
  int slb, rlb, sext, rext;
  char *tmpsend = NULL, *tmprecv = NULL;

  size = lc_size(mv);
  rank = lc_id(mv);
  slb = rlb = 0;
  sext = rext = 1;

  /* Initialization step:
     - if send buffer is not MPI_IN_PLACE, copy send buffer to block 0 of
     receive buffer, else
     - if rank r != 0, copy r^th block from receive buffer to block 0.
     */
  tmpsend = (char*)sbuf;
  tmprecv = (char*)rbuf;
  memcpy(tmprecv, tmpsend, scount * sext);

  /* Communication step:
     At every step i, rank r:
     - doubles the distance
     - sends message which starts at begining of rbuf and has size
     (blockcount * rcount) to rank (r - distance)
     - receives message of size blockcount * rcount from rank (r + distance)
     at location (rbuf + distance * rcount * rext)
     - blockcount doubles until last step when only the remaining data is
     exchanged.
     */
  blockcount = 1;
  tmpsend = (char*)rbuf;
  for (distance = 1; distance < size; distance <<= 1) {
    recvfrom = (rank + distance) % size;
    sendto = (rank - distance + size) % size;

    tmprecv = tmpsend + (ptrdiff_t)distance * (ptrdiff_t)rcount * rext;

    if (distance <= (size >> 1)) {
      blockcount = distance;
    } else {
      blockcount = size - distance;
    }

    lc_req sreq, rreq;

    /* Sendreceive */
    LC_SAFE(lc_send_tag(mv, tmpsend, blockcount * rcount, sendto,
        &MCA_COLL_BASE_TAG_ALLGATHER, &sreq));

    lc_recv_tag(mv, tmprecv, blockcount * rcount, recvfrom,
        &MCA_COLL_BASE_TAG_ALLGATHER, &rreq);

    lc_wait(&sreq);
    lc_wait(&rreq);
  }

  /* Finalization step:
     On all nodes except 0, data needs to be shifted locally:
     - create temporary shift buffer,
     see discussion in coll_basic_reduce.c about the size and begining
     of temporary buffer.
     - copy blocks [0 .. (size - rank - 1)] in rbuf to shift buffer
     - move blocks [(size - rank) .. size] in rbuf to begining of rbuf
     - copy blocks from shift buffer starting at block [rank] in rbuf.
     */
  if (0 != rank) {
    ptrdiff_t true_extent = sext, true_lb = slb;
    char *free_buf = NULL, *shift_buf = NULL;

    free_buf = (char*)calloc(
        ((true_extent + true_lb +
          ((ptrdiff_t)(size - rank) * (ptrdiff_t)rcount - 1) * rext)),
        sizeof(char));

    shift_buf = free_buf - rlb;

    /* 1. copy blocks [0 .. (size - rank - 1)] from rbuf to shift buffer */
    memcpy(shift_buf, rbuf, rext * (size - rank) * rcount);

    /* 2. move blocks [(size - rank) .. size] from rbuf to the begining of rbuf
     */
    tmpsend = (char*)rbuf + (ptrdiff_t)(size - rank) * (ptrdiff_t)rcount * rext;
    memmove(rbuf, tmpsend, rext * (ptrdiff_t)rank * (ptrdiff_t)rcount);

    /* 3. copy blocks from shift buffer back to rbuf starting at block [rank].
     */
    tmprecv = (char*)rbuf + (ptrdiff_t)rank * (ptrdiff_t)rcount * rext;
    __builtin_memcpy(tmprecv, shift_buf,
        rext * (ptrdiff_t)(size - rank) * (ptrdiff_t)rcount);
    free(free_buf);
  }

  return 0;
}

#endif
