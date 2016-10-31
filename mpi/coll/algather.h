#ifndef ALGATHER_H_
#define ALGATHER_H_

#define MCA_COLL_BASE_TAG_ALLGATHER 1338

int ompi_coll_tuned_allgather_intra_bruck(void *sbuf, int scount,
    MPI_Datatype sdtype,
    void* rbuf, int rcount,
    MPI_Datatype rdtype,
    MPI_Comm comm)
{
  int rank, size, sendto, recvfrom, distance, blockcount;
  int slb, rlb, sext, rext;
  char *tmpsend = NULL, *tmprecv = NULL;

  size = MPIV.size;
  rank = MPIV.me;
  slb = rlb = 0;
  MPI_Type_size(sdtype, &sext);
  MPI_Type_size(rdtype, &rext);

  /* Initialization step:
     - if send buffer is not MPI_IN_PLACE, copy send buffer to block 0 of 
     receive buffer, else
     - if rank r != 0, copy r^th block from receive buffer to block 0.
     */
  tmprecv = (char*) rbuf;
  if (MPI_IN_PLACE != sbuf) {
    tmpsend = (char*) sbuf;
    memcpy(tmprecv, tmpsend, scount * sext);
  } else if (0 != rank) {  /* non root with MPI_IN_PLACE */
    tmpsend = ((char*)rbuf) + (ptrdiff_t)rank * (ptrdiff_t)rcount * rext;
    memcpy(tmpsend, tmprecv, rcount * rext);
  }

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
  tmpsend = (char*) rbuf;
  for (distance = 1; distance < size; distance<<=1) {

    recvfrom = (rank + distance) % size;
    sendto = (rank - distance + size) % size;

    tmprecv = tmpsend + (ptrdiff_t)distance * (ptrdiff_t)rcount * rext;

    if (distance <= (size >> 1)) {
      blockcount = distance;
    } else { 
      blockcount = size - distance;
    }

    /* Sendreceive */
    auto t = MPIV_spawn(0, [=](intptr_t) { 
        MPIV_Recv(tmprecv, blockcount * rcount, rdtype, recvfrom, MCA_COLL_BASE_TAG_ALLGATHER + distance, comm, MPI_STATUS_IGNORE);
        });

    MPIV_Send(tmpsend, blockcount * rcount, rdtype, sendto, MCA_COLL_BASE_TAG_ALLGATHER + distance, comm);
    MPIV_join(t);
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
    ptrdiff_t true_extent = sext,  true_lb = slb;
    char *free_buf = NULL, *shift_buf = NULL;

    free_buf = (char*) calloc(((true_extent + true_lb + 
            ((ptrdiff_t)(size - rank) * (ptrdiff_t)rcount - 1) * rext)),
        sizeof(char));

    shift_buf = free_buf - rlb;

    /* 1. copy blocks [0 .. (size - rank - 1)] from rbuf to shift buffer */
    memcpy(shift_buf, rbuf, rext * (size - rank) * rcount);

    /* 2. move blocks [(size - rank) .. size] from rbuf to the begining of rbuf */
    tmpsend = (char*) rbuf + (ptrdiff_t)(size - rank) * (ptrdiff_t)rcount * rext;
    memcpy(rbuf, tmpsend, rext * (ptrdiff_t)rank * (ptrdiff_t)rcount);

    /* 3. copy blocks from shift buffer back to rbuf starting at block [rank]. */
    tmprecv = (char*) rbuf + (ptrdiff_t)rank * (ptrdiff_t)rcount * rext;
    memcpy(tmprecv, shift_buf, rext * (ptrdiff_t)(size - rank) * (ptrdiff_t)rcount);
    free(free_buf);
  }

  return 0;
}

int ompi_coll_tuned_allgather_intra_ring(void *sbuf, int scount,
    MPI_Datatype sdtype,
    void* rbuf, int rcount,
    MPI_Datatype rdtype,
    MPI_Comm comm)
{
  int rank, size;
  int sendto, recvfrom, i, recvdatafrom, senddatafrom;
  int sext, rext;
  char *tmpsend = NULL, *tmprecv = NULL;

  rank = MPIV.me;
  size = MPIV.size;

  MPI_Type_size(sdtype, &sext);
  MPI_Type_size(rdtype, &rext);

  /* Initialization step:
     - if send buffer is not MPI_IN_PLACE, copy send buffer to appropriate block
     of receive buffer
     */
  tmprecv = (char*) rbuf + rank * rcount * rext;
  if (MPI_IN_PLACE != sbuf) {
    tmpsend = (char*) sbuf;
    memcpy(tmprecv, tmpsend, scount * sext);
  }

  /* Communication step:
     At every step i: 0 .. (P-1), rank r:
     - receives message from [(r - 1 + size) % size] containing data from rank
     [(r - i - 1 + size) % size]
     - sends message to rank [(r + 1) % size] containing data from rank
     [(r - i + size) % size]
     - sends message which starts at begining of rbuf and has size 
     */
  sendto = (rank + 1) % size;
  recvfrom  = (rank - 1 + size) % size;

  // fult_t th[size];
  for (i = 0; i < size - 1; i++) {
    recvdatafrom = (rank - i - 1 + size) % size;
    senddatafrom = (rank - i + size) % size;

    tmprecv = (char*)rbuf + recvdatafrom * rcount * rext;
    tmpsend = (char*)rbuf + senddatafrom * rcount * rext;

    auto t = MPIV_spawn(0, [=](intptr_t) { 
        MPIV_Recv(tmprecv, rcount, rdtype, recvfrom, MCA_COLL_BASE_TAG_ALLGATHER + i, comm, MPI_STATUS_IGNORE);
        });

    MPIV_Send(tmpsend, rcount, rdtype, sendto, MCA_COLL_BASE_TAG_ALLGATHER + i, comm);
    MPIV_join(t);
  }

  // for (i = 0; i < size - 1; i++) {
  // MPIV_join(th[i]);
  // }

  return 0;
}

void MPIV_Allgather(void *sbuf, int scount,
    MPI_Datatype sdtype,
    void* rbuf, int rcount,
    MPI_Datatype rdtype,
    MPI_Comm comm) {
  ompi_coll_tuned_allgather_intra_bruck(sbuf, scount, sdtype,
      rbuf, rcount, rdtype, comm);
}

#endif
