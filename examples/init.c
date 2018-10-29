#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  lc_ep ep;
  lc_init(1, &ep);
  lc_finalize();
}
