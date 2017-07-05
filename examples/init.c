#include "lc.h"

int main(int argc, char** args) {
  lch* mv;
  lc_open(&mv);
  lc_close(mv);
}
