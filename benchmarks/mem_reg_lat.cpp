#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include "lci.h"
#include "comm_exp.h"

/**
 * Ping-pong benchmark with sendbc/recvbc
 */

LCI_endpoint_t ep;

int main(int argc, char *argv[]) {
  int min_size = 4 * 1024;
  int max_size = 2 * 1024 * 1024;
  int loop = 1000;
  if (argc > 1)
    min_size = atoi(argv[1]);
  if (argc > 2)
    max_size = atoi(argv[2]);
  if (argc > 3)
    loop = atoi(argv[3]);

  LCI_initialize();

  for (int size = min_size; size <= max_size; size <<= 1) {
    double t;
    void *buffer = malloc(size);
    t = wtime();
    for (int i = 0; i < loop; ++i) {
      LCI_segment_t segment;
      LCI_memory_register(LCI_UR_DEVICE, buffer, size, &segment);
      LCI_memory_deregister(&segment);
    }
    t = wtime() - t;
    free(buffer);

    printf("loop %d, size %d: average time %lf us\n", loop, size, t / loop * 1e6);
  }

  for (int size = min_size; size <= max_size; size <<= 1) {
    double t;
    char *buffer = (char*) malloc(size * loop);
    t = wtime();
    for (int i = 0; i < loop; ++i) {
      LCI_segment_t segment;
      LCI_memory_register(LCI_UR_DEVICE, buffer + i * size, size, &segment);
      LCI_memory_deregister(&segment);
    }
    t = wtime() - t;
    free(buffer);

    printf("loop %d, size %d: average time %lf us\n", loop, size, t / loop * 1e6);
  }

  LCI_finalize();
  return EXIT_SUCCESS;
}