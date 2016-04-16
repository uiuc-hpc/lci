#ifndef BARRIER_H_
#define BARRIER_H_

const int MPIV_COLL_BASE_TAG_BARRIER = 413371337;

void MPIV_Barrier_log(MPI_Comm comm) {
#if USE_MPE
  MPE_Log_event(mpiv_barrier_start, 0, "start_barrier");
#endif

  int i, peer, dim, hibit, mask;

  int size = MPIV.size;
  int rank = MPIV.me;

  /* Send null-messages up and down the tree.  Synchronization at the
   * root (rank 0). */

  dim = opal_cube_dim(size);
  hibit = opal_hibit(rank, dim);
  --dim;

  /* Receive from children. */
  for (i = dim, mask = 1 << i; i > hibit; --i, mask >>= 1) {
    peer = rank | mask;
    if (peer < size) {
      MPIV_Recv(0, 0, MPI_BYTE, peer, MPIV_COLL_BASE_TAG_BARRIER,
          MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
  }

  /* Send to and receive from parent. */
  if (rank > 0) {
    peer = rank & ~(1 << hibit);
    MPIV_Send(0, 0, MPI_BYTE, peer, MPIV_COLL_BASE_TAG_BARRIER, comm);
    MPIV_Recv(0, 0, MPI_BYTE, peer, MPIV_COLL_BASE_TAG_BARRIER, comm,
              MPI_STATUS_IGNORE);
  }

  for (i = hibit + 1, mask = 1 << i; i <= dim; ++i, mask <<= 1) {
    peer = rank | mask;
    if (peer < size) {
      MPIV_Send(0, 0, MPI_BYTE, peer, MPIV_COLL_BASE_TAG_BARRIER, comm);
    }
  }

#if USE_MPE
  MPE_Log_event(mpiv_barrier_end, 0, "end_barrier");
#endif
}

#define MCA_PML_CALL(x) MPIV_ ## x
#define MCA_COLL_BASE_TAG_BARRIER MPIV_COLL_BASE_TAG_BARRIER

int MPIV_Barrier_N(MPI_Comm comm) {
  int i;
  int size = MPIV.size;
  int rank = MPIV.me;

  /* All non-root send & receive zero-length message. */

  if (rank > 0) {
    MCA_PML_CALL(Send
        (NULL, 0, MPI_BYTE, 0, MCA_COLL_BASE_TAG_BARRIER,comm));

    MCA_PML_CALL(Recv
        (NULL, 0, MPI_BYTE, 0, MCA_COLL_BASE_TAG_BARRIER, comm, MPI_STATUS_IGNORE));
  }

  /* The root collects and broadcasts the messages. */

  else {
    fult_t th[size];
    for (i = 1; i < size; ++i) {
      th[i] = MPIV_spawn(0, [](intptr_t i) {
          MCA_PML_CALL(Recv(NULL, 0, MPI_BYTE, i,
                MCA_COLL_BASE_TAG_BARRIER,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE));
          MCA_PML_CALL(Send
              (NULL, 0, MPI_BYTE, i,
               MCA_COLL_BASE_TAG_BARRIER, MPI_COMM_WORLD));
      }, i);
    }
    for (i = 1; i < size; ++i) {
      MPIV_join(th[i]);
    }
  }

  /* All done */

  return MPI_SUCCESS;
}

void MPIV_Barrier(MPI_Comm comm) {
  if (MPIV.size > 32)
    MPIV_Barrier_log(comm);
  else
    MPIV_Barrier_N(comm);
}

#endif

