#include "lc.h"
#include <sys/mman.h>

int main(int argc, char** args) {
  lch* mv;
  lc_open(&mv);
  lc_close(mv);
  printf("Ok\n");
}
