// This file test the rdmax post_send, each thread sends the same amount of data
// over a number of times.

#include <future>
#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <thread>

#include "rdmax.h"

#define USE_AFFI
#include "affinity.h"

#define USE_PAPI
#include "profiler.h"

using rdmax::connection;
using rdmax::device;
using rdmax::device_cq;
using rdmax::device_ctx;
using rdmax::device_memory;

int main(int argc, char** args) {
  MPI_Init(&argc, &args);
  int rank = 0;
  int nnode = 0;
  MPI_Comm_size(MPI_COMM_WORLD, &nnode);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  assert(nnode == 2 && "There must be 2 nodes");

  if (argc < 2) {
    if (rank == 0)
      std::cerr << "Usage: " << args[0] << " <size per thread> <nthreads>"
                << std::endl;
    return 0;
  }

  size_t msg_size = atoi(args[1]);
  int num_send = 1000;

  assert(num_send < 10000 && "Not enough resources for this");

  if (rank == 0) {
    std::cout << "Testing ... size " << msg_size << std::endl;
  }

  vector<device> devs = rdmax::device::get_devices();
  assert(devs.size() > 0 && "Unable to find any ibv device");

  device_ctx ctx(devs[1]);

  if (rank == 0) {
    std::cout << "Num device: " << devs.size() << std::endl;
    std::cout << "Using device " << devs[1].name() << std::endl;
  }

  // Create completion channel.
  device_cq cq = ctx.create_cq(num_send);

  // Create RDMA memory.
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

  device_memory dm = ctx.create_memory(msg_size * 100, mr_flags);

  // Connect to the other rank.
  connection conn(&cq, &cq, &ctx, &dm, 1 - rank);

  // Initialize the send buffer.
  char* ptr = static_cast<char*>(dm.ptr());
  affinity::set_me_to(0);

  // profiler p({PAPI_L1_DCM});

  MPI_Barrier(MPI_COMM_WORLD);
  double t_all = 0;
  long long s = 0;

  if (rank == 1) {
    for (int j = 0; j < num_send + 100; ++j) {
      if (j >= 100) {
        // p.start();
        t_all -= MPI_Wtime();
      }
      ctx.post_srq_recv(ptr, ptr, msg_size, dm.lkey());
      while (!cq.poll_once([](ibv_wc&) {}))
        ;
      memset(ptr, 'A', msg_size);
      conn.write_send(ptr, msg_size, dm.lkey(), ptr);
      while (!cq.poll_once([](ibv_wc&) {}))
        ;
      if (j >= 100) {
        t_all += MPI_Wtime();
        // auto v = p.stop();
        // s += v[0];
      }
    }
  } else {
    for (int j = 0; j < num_send + 100; ++j) {
      // ptr affinity == 0
      memset(ptr, 'B', msg_size);

      // if (argc == 3) {
      /* } else {
        auto t = std::thread([&] {
          affinity::set_me_to(0);
          memset(ptr, 'A', msg_size);
        });
        t.join();
      }*/
      auto t = std::thread([&] {
        affinity::set_me_to(0);
        conn.write_send(ptr, msg_size, dm.lkey(), ptr);
        while (!cq.poll_once([](ibv_wc&) {}))
          ;
        memset(ptr, 'A', msg_size);
        // ptr affinity == 4
      });
      t.join();

      // for (int i = 0; i < msg_size; i++)
      // assert(ptr[i]  == 'A');

      if (argc == 3) {
        auto t = std::thread([&] {
          affinity::set_me_to(4);
          ctx.post_srq_recv(ptr, ptr, msg_size, dm.lkey());
          while (!cq.poll_once([](ibv_wc&) {}))
            ;

          if (j >= 100) t_all -= MPI_Wtime();
          for (int i = 0; i < msg_size; i++) assert(ptr[i] == 'A');
          if (j >= 100) t_all += MPI_Wtime();
        });
        t.join();
      } else {
        auto t = std::thread([&] {
          affinity::set_me_to(0);
          ctx.post_srq_recv(ptr, ptr, msg_size, dm.lkey());
          while (!cq.poll_once([](ibv_wc&) {}))
            ;

          if (j >= 100) t_all -= MPI_Wtime();
          for (int i = 0; i < msg_size; i++) assert(ptr[i] == 'A');
          if (j >= 100) t_all += MPI_Wtime();
        });
        t.join();
      }
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    std::cout << rank << " written something! Time per write: " << std::fixed
              << std::setprecision(9)
              << "Overall latency: " << 1e6 * t_all / num_send << " us, "
              << std::endl
              << "Overall BW: " << msg_size * num_send / t_all / 1e6 << " MB/s "
              << std::endl
              << "Miss: " << 1.0 * s / num_send << std::endl;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return 0;
}
