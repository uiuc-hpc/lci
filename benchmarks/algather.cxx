#include "mpiv.h"
#include "comm_exp.h"

int main(int argc, char** args) {
  MPIV_Init(argc, args);
  MPIV_Init_worker(1);
  MPIV_Finalize();
}

// #undef TOTAL_LARGE
// #undef SKIP_LARGE

// #define TOTAL_LARGE 1
// #define SKIP_LARGE 0 

void main_task(intptr_t args) {
  double t = 0;
  int size = 1024;
  void* recvbuf = mpiv_malloc(size * MPIV.size);

  for (int i = 0; i < TOTAL_LARGE + SKIP_LARGE; i++) {
    double t1=wtime();
    MPIV_Allgather(MPI_IN_PLACE, size, MPI_CHAR, recvbuf, size, MPI_CHAR, MPI_COMM_WORLD);
    t1 = wtime() - t1;
    if (i >= SKIP_LARGE) 
      t += t1;
    MPIV_Barrier(MPI_COMM_WORLD);
  }

  if (MPIV.me == 0) 
    std::cout << "Time: " << 1e6 * t / TOTAL_LARGE << std::endl;
  mpiv_free(recvbuf);
}
