#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mv.h"
#define MV_USE_SERVER_OFI
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

  int WINDOWS = atoi(args[1]);

  mv_packet* p_send[WINDOWS];
  mv_packet* p_recv[WINDOWS];

  for (int i = 0; i < WINDOWS; i++) {
    p_send[i] = mv_pool_get(mv->pkpool);
    p_recv[i] = mv_pool_get(mv->pkpool);
  }
    // mv_server_post_recv(mv->server, mv_pool_get(mv->pkpool));

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // printf("%d\n", server_max_inline);

  double times[NEXP];
  double times_p[NEXP];
  int count = 0;

  void* src = malloc(8192);
#define MIN_MSG_SIZE 64
#define MAX_MSG_SIZE 64
  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    double time = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    for (int exp = 0; exp < NEXP; exp ++) {
      if (rank == 0)  {
        for (int i = 0; i < TOTAL + SKIP; i++) {
          if (i == SKIP) {
            times[exp] = MPI_Wtime();
            times_p[exp] = 0;
          }
          times_p[exp] -= MPI_Wtime();
          // memcpy((void*) &p_send->data, src, size);
          for (int k = 0; k < WINDOWS; k++)
            mv_server_send(mv->server, 1-rank, &p_send[k]->data,
              (size_t)(size + sizeof(packet_header)), &p_send[k]->context);
          times_p[exp] += MPI_Wtime();
          count = 0;
          while (count < WINDOWS) {
            count += mv_server_progress_once(mv->server);
          }
#if 0
      //    times_p[exp] += MPI_Wtime();
          for (int k = 0; k < WINDOWS; k++)
            mv_server_post_recv(mv->server, p_recv[k]);
           // times_p[exp] -= MPI_Wtime();
          count = 0;
          while (count < WINDOWS) {
            count += mv_server_progress_once(mv->server);
          }
#endif
        }
      } else {
        for (int i = 0; i < TOTAL + SKIP; i++) {
          for (int k = 0; k < WINDOWS; k++)
            mv_server_post_recv(mv->server, p_recv[k]);
          count = 0;
          while (count < WINDOWS) {
            count += mv_server_progress_once(mv->server);
          }
#if 0
          for (int k = 0; k < WINDOWS; k++)
            mv_server_send(mv->server, 1-rank, &p_send[k]->data,
              (size_t)(size + sizeof(packet_header)), &p_send[k]->context);
          count = 0;
          while (count < WINDOWS) {
            count += mv_server_progress_once(mv->server);
          }
#endif
        }
      }
      times[exp] = (MPI_Wtime() - times[exp]) * 1e6 / TOTAL / WINDOWS;
      times_p[exp] = times_p[exp] * 1e6 / TOTAL / WINDOWS;
      MPI_Barrier(MPI_COMM_WORLD);
    }
    if (rank == 0) {
      double sum = 0;
      for (int i = 0; i < NEXP; i++)
        sum += times_p[i];
      double mean = sum / NEXP;
      sum = 0;
      for (int i = 0; i < NEXP; i++)
        sum += (times_p[i] - mean) * (times_p[i] - mean);
      double std = sqrt(sum / (NEXP - 1));
      printf("%d %.2f %.2f %.2f\n", size, mean, std, times_p[0]);
    }
  }

  free(src);
  mv_close(mv);
  return 0;
}

void main_task(intptr_t arg) { }

