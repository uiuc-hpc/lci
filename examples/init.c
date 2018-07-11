#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  lc_hw hw;
  lc_ep ep;
  lc_rep rep;
  lc_init();
  lc_hw_open(&hw);
  lc_ep_open(hw, EP_TYPE_QUEUE, &ep);
  lc_ep_connect(ep, 1-lc_rank(), 0, &rep);
  lc_finalize();
}
