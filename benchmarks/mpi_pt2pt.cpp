#include <iostream>
#include <mpi.h>
#define LCIT_BENCH
#include "lcit.h"

#define MPI_CHECK(stmt)                                               \
  do {                                                                \
    int mpi_errno = (stmt);                                           \
    if (MPI_SUCCESS != mpi_errno) {                                   \
      fprintf(stderr, "[%s:%d] MPI call failed with %d \n", __FILE__, \
              __LINE__, mpi_errno);                                   \
      exit(EXIT_FAILURE);                                             \
    }                                                                 \
  } while (0)

using namespace lcit;
int rank, nranks;

void test(Context ctx)
{
  int tag = 245 + TRD_RANK_ME;
  //  int peer_rank = ((1 - rank % 2) + rank / 2 * 2) % nranks; // 0 <-> 1, 2
  //  <-> 3
  int peer_rank = (rank + nranks / 2) % nranks;  // 0 <-> 2 1 <-> 3
  char* send_buf;
  char* recv_buf;
  const int PAGE_SIZE = sysconf(_SC_PAGESIZE);
  posix_memalign((void**)&send_buf, PAGE_SIZE, ctx.config.max_msg_size);
  posix_memalign((void**)&recv_buf, PAGE_SIZE, ctx.config.max_msg_size);
  std::vector<MPI_Request> comps;

  //  int rc;
  //  char hostname[50];
  //  rc = gethostname(hostname,sizeof(hostname));
  //  if(rc == 0){
  //    printf("rank %d/%d -> %d hostname = %s\n",rank, nranks, peer_rank,
  //    hostname);
  //  }

  //  if (rank % 2 == 0) {
  if (rank < nranks / 2) {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size;
         size <<= 1) {
      threadBarrier(ctx);

      double time = RUN_VARY_MSG(ctx, [&]() {
        // send
        comps.resize(ctx.config.send_window);
        for (int j = 0; j < ctx.config.send_window; ++j) {
          MPI_CHECK(MPI_Isend(send_buf, size, MPI_CHAR, peer_rank, tag,
                              MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
        // recv
        comps.resize(ctx.config.recv_window);
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          MPI_CHECK(MPI_Irecv(recv_buf, size, MPI_CHAR, peer_rank, tag,
                              MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
      });
      if (TRD_RANK_ME == 0 && rank == 0) {
        int worker_thread_num = ctx.config.nthreads * nranks / 2;
        double latency_us = time * 1e6;
        double msg_rate_mps =
            ctx.config.send_window / latency_us * worker_thread_num;
        double bw_mbps = size * msg_rate_mps;
        printf("%d %.2f %.2f %.2f\n", size, latency_us, msg_rate_mps, bw_mbps);
      }
    }
  } else {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size;
         size <<= 1) {
      threadBarrier(ctx);
      RUN_VARY_MSG(ctx, [&]() {
        // recv
        comps.resize(ctx.config.send_window);
        for (int j = 0; j < ctx.config.send_window; ++j) {
          MPI_CHECK(MPI_Irecv(recv_buf, size, MPI_CHAR, peer_rank, tag,
                              MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
        // send
        comps.resize(ctx.config.recv_window);
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          MPI_CHECK(MPI_Isend(send_buf, size, MPI_CHAR, peer_rank, tag,
                              MPI_COMM_WORLD, &comps[j]));
        }
        MPI_Waitall(comps.size(), comps.data(), MPI_STATUS_IGNORE);
      });
    }
  }
}

void worker_handler_mpi(int id, Context ctx)
{
  TRD_RANK_ME = id;
  to_progress = false;
  test(ctx);
  //  std::invoke(std::forward<Fn>(fn),
  //              std::forward<Args>(args)...);
}

int main(int argc, char** args)
{
  Config config = parseArgs(argc, args);
  Context ctx;
  ctx.config = config;
  int provided;
  if (ctx.config.nthreads > 1) {
    ctx.threadBarrier = new ThreadBarrier(config.nthreads);
    MPI_CHECK(
        MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided));
  } else {
    MPI_CHECK(MPI_Init_thread(nullptr, nullptr, MPI_THREAD_SINGLE, &provided));
  }
  MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &nranks));
  MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  if (rank == 0) printConfig(config);

  // run the code
  std::vector<std::thread> worker_pool;
  if (ctx.config.nthreads > 1) {
    // Multithreaded version
    // initialize worker threads
    for (size_t i = 0; i < ctx.config.nthreads; ++i) {
      std::thread t(worker_handler_mpi, i, ctx);
      if (ctx.config.thread_pin)
        set_affinity(t.native_handle(), i % NPROCESSORS);
      worker_pool.push_back(std::move(t));
    }
    // wait for workers to finish
    for (size_t i = 0; i < ctx.config.nthreads; ++i) {
      worker_pool[i].join();
    }
  } else {
    // Singlethreaded version
    test(ctx);
  }

  MPI_Finalize();
  return 0;
}
