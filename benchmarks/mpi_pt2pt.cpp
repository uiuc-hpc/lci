#include <iostream>
#include <mpi.h>
#define LCIT_BENCH
#include "lcit.h"

#define MPI_CHECK(stmt)                                          \
do {                                                             \
   int mpi_errno = (stmt);                                       \
   if (MPI_SUCCESS != mpi_errno) {                               \
       fprintf(stderr, "[%s:%d] MPI call failed with %d \n",     \
        __FILE__, __LINE__,mpi_errno);                           \
       exit(EXIT_FAILURE);                                       \
   }                                                             \
} while (0)

using namespace lcit;
int rank, nranks;

void test(Context ctx) {
  int tag = 245;
  int peer_rank = ((1 - rank % 2) + rank / 2 * 2) % nranks;
  char *send_buf;
  char *recv_buf;
  const int PAGE_SIZE = sysconf(_SC_PAGESIZE);
  posix_memalign((void **)&send_buf, PAGE_SIZE, ctx.config.max_msg_size);
  posix_memalign((void **)&recv_buf, PAGE_SIZE, ctx.config.max_msg_size);
  std::vector<MPI_Request> comps;

  if (rank % 2 == 0) {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      threadBarrier(ctx);

      double time = RUN_VARY_MSG(ctx, [&]() {
        // send
        comps.resize(ctx.config.send_window);
        for (int j = 0; j < ctx.config.send_window; ++j) {
          MPI_CHECK(MPI_Isend(send_buf, size, MPI_CHAR, peer_rank, tag, MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
        // recv
        comps.resize(ctx.config.recv_window);
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          MPI_CHECK(MPI_Irecv(recv_buf, size, MPI_CHAR, peer_rank, tag, MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
      });
      if (TRD_RANK_ME == 0 && rank == 0) {
        int worker_thread_num = ctx.config.nthreads == 1? 1 : ctx.config.nthreads - 1;
        double bw = size * ctx.config.send_window / time / 1e6 * worker_thread_num;
        printf("%d %.2f %.2f\n", size, time * 1e6, bw);
      }
    }
  } else {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      threadBarrier(ctx);
      RUN_VARY_MSG(ctx, [&]() {
        // recv
        comps.resize(ctx.config.send_window);
        for (int j = 0; j < ctx.config.send_window; ++j) {
          MPI_CHECK(MPI_Irecv(recv_buf, size, MPI_CHAR, peer_rank, tag, MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
        // send
        comps.resize(ctx.config.recv_window);
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          MPI_CHECK(MPI_Isend(send_buf, size, MPI_CHAR, peer_rank, tag, MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
      });
    }
  }
}

int main(int argc, char** args) {
  MPI_Init(0, 0);
  MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &nranks));
  MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  Config config = parseArgs(argc, args);
  if (rank == 0)
    printConfig(config);
  Context ctx;
  ctx.config = config;

  test(ctx);

  MPI_Finalize();
  return 0;
}
