#include <stdio.h>

int main() {
  for (int i = 1; i < 4*1024*1024; i*=2) {
    void* b = malloc(1024);
    fprintf(stderr, "%p\n", b);
  }
}
