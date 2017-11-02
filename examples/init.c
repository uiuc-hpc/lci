#include "lc.h"
#include <sys/mman.h>

int main(int argc, char** args) {
  lch* mv;
  lc_open(&mv);
  printf("%d %d\n", lc_id(mv), lc_size(mv));
  lc_close(mv);
  printf("OK\n");
}
