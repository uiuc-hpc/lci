// This file test the rdmax post_send, each thread sends the same amount of data
// over a number of times.

#include <mpi.h>
#include <iostream>
#include <future>
#include <iomanip>
#include <cstring>
#include <assert.h>
#include <vector>

int main(int argc, char** args)
{
  int provided;
  MPI_Init_thread(&argc, &args, MPI_THREAD_MULTIPLE, &provided);
  int rank = 0;
  int nnode = 0;
  MPI_Comm_size(MPI_COMM_WORLD, &nnode);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  assert(nnode == 2 && "There must be 2 nodes");

  if (argc < 3) {
    if (rank == 0)
      std::cerr << "Usage: " << args[0] << " <size per thread> <nthreads>"
                << std::endl;
    return 0;
  }

  int msg_size_per_thread = atoi(args[1]);
  int num_threads = atoi(args[2]);

  int num_send = num_threads * 100;
  int msg_size = msg_size_per_thread * num_send;

  assert(num_send < 10000 && "Not enough resources for this");

  if (rank == 0) {
    std::cout << "Testing MPI ... size " << msg_size << ", threads "
              << num_threads << std::endl;
  }

  char* ptr = new char[msg_size];
  MPI_Win win;
  MPI_Win_create(ptr, msg_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);

  // Initialize the send buffer.
  std::string msg(msg_size - 1, 'A');

  if (rank == 0) {
    memcpy(ptr, msg.c_str(), msg_size);
    ptr[msg_size - 1] = 0;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  double t_all = MPI_Wtime();
  double t_put = 0;

  if (rank == 0) {
    std::vector<std::future<double>> th(num_threads);
    MPI_Win_lock(MPI_LOCK_SHARED, 1, 0, win);

    volatile bool stop = false;
    auto poll_thread = std::thread([&win, &stop] {
      while (!stop) {
        MPI_Win_flush(1, win);
      }
    });

    // Do this for num_send/num_threads times, i.e. num_send requests submitted.
    for (int i = 0; i < num_send; i += num_threads) {
      // Each thread will submit a send request using the connection.
      for (int j = 0; j < num_threads; ++j) {
        th[j] = std::move(std::async(
            std::launch::async, [&win, &ptr, i, j, num_send, msg_size] {
              double t = MPI_Wtime();
              int offset = (msg_size / num_send) * (i + j);
              MPI_Put(ptr + offset, msg_size / num_send, MPI_CHAR, 1, offset,
                      msg_size / num_send, MPI_CHAR, win);
              t = MPI_Wtime() - t;
              return t;
            }));
      }

      // Synchronize threads and get the timing.
      for (auto& t : th) t_put += t.get();
    }
    stop = true;
    poll_thread.join();
    MPI_Win_unlock(1, win);
  }

  // If rank 0 reaches here it should be done.
  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    t_all = MPI_Wtime() - t_all;
    std::cout << rank << " written something! Time per write: " << std::fixed
              << std::setprecision(9) << 1e6 * t_put / num_send << " us, "
              << "Overall BW: " << msg_size / t_all / 1024 / 1024 << " MB/s "
              << std::endl;
  }

  // Rank 1 check the result from device memory.
  if (rank == 1) {
    int pos = msg.compare(ptr);
    if (pos == 0) {
      std::cout << rank << " received correct! " << std::endl;
    } else {
      std::cerr << "Failed at" << pos << std::endl;
    }
  }

  MPI_Win_free(&win);
  MPI_Finalize();
  return 0;
}
