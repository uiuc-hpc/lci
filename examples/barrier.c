#include "lc.h"
#include "comm_exp.h"
#include <assert.h>
#include <string.h>

int main(int argc, char** args) {
  lch* mv;
  lc_open(&mv);

  int t = lc_id(mv);
  double t1;
  int i;
  for (i = 0; i < TOTAL + SKIP; i++) {
    if (i == SKIP) {
      t1 = wtime();
    }

    lc_barrier(mv); 
  }

  t1 = wtime() - t1;

  if (t == 0)
    printf("%.5f (usec)\n", 1e6 * t1 / TOTAL);


  lc_close(mv);
}
