#include "comm_exp.h"
#include "mpiv.h"

int main(int argc, char** args) {
  MPIV_Init(&argc, &args);
  MPIV_Init_worker(1);
  MPIV_Finalize();
}

void main_task(intptr_t args) {
  double t = 0;
  for (int i = 0; i < 110; i++) {
    double t1 = wtime();
    MPIV_Barrier(MPI_COMM_WORLD);
    t1 = wtime() - t1;
    if (i >= 10) t += t1;
  }
  if (MPIV.me == 0) std::cout << "Time: " << 1e6 * t / 100 << std::endl;
}
