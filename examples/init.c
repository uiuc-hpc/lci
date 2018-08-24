#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  lc_ep ep;
  lc_init(1, EP_AR_EXPL, EP_CE_CQ, &ep);
  lc_finalize();
}
