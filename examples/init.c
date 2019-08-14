#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  LCI_initialize(&argc, &args);
  LCI_finalize();
}
