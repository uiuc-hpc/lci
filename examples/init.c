#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  LCI_Initialize(1);
  LCI_Finalize();
}
