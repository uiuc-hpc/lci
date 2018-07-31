#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  lc_dev dev;
  lc_ep ep;
  lc_rep rep;
  lc_init();
  lc_dev_open(&dev);
  lc_ep_open(dev, EP_TYPE_QUEUE, &ep);
  lc_ep_query(dev, 1-lc_rank(), 0, &rep);
  lc_finalize();
}
