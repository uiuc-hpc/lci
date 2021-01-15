#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mv.h"
#include "src/include/mv_priv.h"

// #define USE_L1_MASK
#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

#include "comm_exp.h"

mvh* mv;

int main(int argc, char** args)
{
  size_t heap_size = 1024 * 1024 * 1024;
  mv_open(&argc, &args, heap_size, &mv);
  set_me_to_last();

  mv_packet* p_send = mv_pool_get(mv->pkpool);
  for (int i = 0; i < 1; i++)
    mv_server_post_recv(mv->server, mv_pool_get(mv->pkpool));

  mv_packet* p_recv = mv_pool_get(mv->pkpool);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // printf("%d\n", server_max_inline);

  double times[NEXP];

  void* src = malloc(8192);
#define MIN_MSG_SIZE 64
#define MAX_MSG_SIZE 64
  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    double time = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    for (int exp = 0; exp < NEXP; exp ++) {
      if (rank == 0)  {
        for (int i = 0; i < TOTAL + SKIP; i++) {
          if (i == SKIP)
            times[exp] = MPI_Wtime();
          memcpy((void*) &p_send->data, src, size);
          int k = mv_server_send(mv->server, 1-rank, &p_send->data,
              (size_t)(size + sizeof(struct packet_header)), &p_send->context);
#ifdef MV_USE_SERVER_OFI
          if (k)
#endif
            while (mv_server_progress_once(mv->server) == 0);
          mv_server_post_recv(mv->server, p_recv);
          while (mv_server_progress_once(mv->server) == 0);
        }
      } else {
        for (int i = 0; i < TOTAL + SKIP; i++) {
          mv_server_post_recv(mv->server, p_recv);
          while (mv_server_progress_once(mv->server) == 0);
          memcpy((void*) &p_send->data, src, size);
          int k = mv_server_send(mv->server, 1-rank, &p_send->data,
              (size_t)(size + sizeof(struct packet_header)), &p_send->context);
#ifdef MV_USE_SERVER_OFI
          if (k)
#endif
            while (mv_server_progress_once(mv->server) == 0);
        }
      }
      times[exp] = (MPI_Wtime() - times[exp]) * 1e6 / TOTAL / 2;
      MPI_Barrier(MPI_COMM_WORLD);
    }
    if (rank == 0) {
      double sum = 0;
      for (int i = 0; i < NEXP; i++)
        sum += times[i];
      double mean = sum / NEXP;
      sum = 0;
      for (int i = 0; i < NEXP; i++)
        sum += (times[i] - mean) * (times[i] - mean);
      double std = sqrt(sum / (10 - 1));
      printf("%d %.2f %.2f\n", size, mean, std);
    }
  }

  free(src);
  mv_close(mv);
  return 0;
}

void main_task(intptr_t arg) { }

