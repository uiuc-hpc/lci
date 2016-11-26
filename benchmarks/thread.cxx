#include "comm_exp.h"
#include "mpiv.h"

int main(int argc, char** args) {
  MPIV_Init(argc, args);
  MPIV_Init_worker(1);
  MPIV_Finalize();
}

void main_task(intptr_t args) {
  double t = 0;
  for (int i = 0; i < TOTAL_LARGE + SKIP_LARGE; i++) {
    if (i == SKIP_LARGE) t = MPIV_Wtime();
    fult_t f = MPIV_spawn(0, [&t](intptr_t) { t += 1; });
    MPIV_join(f);
    t -= 1;
  }
  t = MPIV_Wtime() - t;
  if (MPIV.me == 0) std::cout << "Time: " << 1e6 * t / TOTAL_LARGE << std::endl;
}
