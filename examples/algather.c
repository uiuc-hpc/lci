#include "lc.h"
#include "comm_exp.h"
#include <assert.h>
#include <string.h>

int main(int argc, char** args) {
  lch* mv;
  lc_open(&mv);

  int t = lc_id(mv);
  int* dst = calloc(sizeof(int), lc_size(mv));

  double t1;
  int i;
  for (i = 0; i < TOTAL + SKIP; i++) {
    if (i == SKIP) {
      t1 = wtime();
      memset(dst, sizeof(int)*lc_size(mv), 0);
    }

    lc_algather(&t, sizeof(int), dst, sizeof(int), mv);
    
    if (i == SKIP) {
      int j;
      for (j = 0; j < lc_size(mv); j++) {
        if (dst[j] != j) {
          printf("%d> %d invalid %d\n", t, j, dst[j]);
        }
      }
    }
  }

  t1 = wtime() - t1;

  if (t == 0)
    printf("%.5f (usec)\n", 1e6 * t1 / TOTAL);


  lc_close(mv);
}
