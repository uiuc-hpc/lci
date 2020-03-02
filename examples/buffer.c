#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  LCI_initialize(&argc, &args);
  LCI_bbuffer_t bbuffer;
  LCI_bbuffer_get(&bbuffer, 0);
  for (int i = 0; i < 1024; i++) {
    ((char*) bbuffer)[i] = 'A';
  }
  LCI_bbuffer_free(bbuffer, 0);
  LCI_finalize();
}
