#ifndef COLLECTIVE_H_
#define COLLECTIVE_H_

static int opal_cube_dim(int value) {
  int dim, size;

  for (dim = 0, size = 1; size < value; ++dim, size <<= 1) {
    continue;
  }

  return dim;
}

static inline int opal_hibit(int value, int start) {
  unsigned int mask;

  --start;
  mask = 1 << start;

  for (; start >= 0; --start, mask >>= 1) {
    if (value & mask) {
      break;
    }
  }

  return start;
}

const int MPIV_COLL_BASE_TAG_BARRIER = 1337;

#if 1
void MPIV_Barrier(MPI_Comm comm) {
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
#else

static inline int opal_next_poweroftwo_inclusive(int value) {
  int power2;

#if OPAL_C_HAVE_BUILTIN_CLZ
  if (OPAL_UNLIKELY(1 >= value)) {
    return 1;
  }
  power2 = 1 << (8 * sizeof(int) - __builtin_clz(value - 1));
#else
  for (power2 = 1; power2 < value; power2 <<= 1) /* empty */
    ;
#endif

  return power2;
}

void MPIV_Barrier(MPI_Comm comm) {
  int rank, size, depth, err, jump, partner;

  rank = MPIV.me;
  size = MPIV.size;

  /* Find the nearest power of 2 of the communicator size. */
  depth = opal_next_poweroftwo_inclusive(size);

  for (jump = 1; jump < depth; jump <<= 1) {
    partner = rank ^ jump;
    if (!(partner & (jump - 1)) && partner < size) {
      if (partner > rank) {
        MPIV_Recv(0, 0, MPI_BYTE, partner, MPIV_COLL_BASE_TAG_BARRIER, comm,
                  MPI_STATUS_IGNORE);
      } else if (partner < rank) {
        MPIV_Send(0, 0, MPI_BYTE, partner, MPIV_COLL_BASE_TAG_BARRIER, comm);
      }
    }
  }

  depth >>= 1;
  for (jump = depth; jump > 0; jump >>= 1) {
    partner = rank ^ jump;
    if (!(partner & (jump - 1)) && partner < size) {
      if (partner > rank) {
        MPIV_Send(0, 0, MPI_BYTE, partner, MPIV_COLL_BASE_TAG_BARRIER, comm);
      } else if (partner < rank) {
        MPIV_Recv(0, 0, MPI_BYTE, partner, MPIV_COLL_BASE_TAG_BARRIER, comm,
                  MPI_STATUS_IGNORE);
      }
    }
  }
}
#endif

#endif
