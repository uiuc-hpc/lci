#include "mv.h"
#include "mv/helper.h"
#include "comm_exp.h"

#include <assert.h>

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1<<22)

mvh* mv;

int main(int argc, char** args)
{
  mv_open(1024 * 1024 * 1024, &mv);
  char* buf = (char*) malloc(MAX_MSG_SIZE);
  mv_addr* rma;
  mv_rma_create(mv, buf, MAX_MSG_SIZE, &rma);
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  void* buf2;
  if (rank == 0) {
    mv_ctx s;
    mv_send_queue(mv, rma, sizeof(mv_addr), 1, 0, &s);
    while (!mv_test(&s))
      mv_progress(mv);

    int size, rank, tag;
    mv_ctx r;
    while (!mv_recv_queue(mv, &size, &rank, &tag, &r))
      mv_progress(mv);
    buf2 = malloc(size);
    mv_recv_queue_post(mv, buf2, &r);
  } else {
    int size, rank, tag;
    mv_ctx r;
    while (!mv_recv_queue(mv, &size, &rank, &tag, &r))
      mv_progress(mv);
    buf2 = malloc(size);
    mv_recv_queue_post(mv, buf2, &r);

    mv_ctx s;
    mv_send_queue(mv, rma, sizeof(mv_addr), 0, 0, &s);
    while (!mv_test(&s))
      mv_progress(mv);
  }

  void* src = malloc(MAX_MSG_SIZE);
  memset(src, 'A', MAX_MSG_SIZE);
  mv_addr* remote_rma = (mv_addr*) buf2;
  mv_ctx c1, c2;
  mv_recv_put_signal(mv, rma, &c2);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    double t1 = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (rank == 0) {
        if (i == SKIP)
          t1 = MPI_Wtime();

        mv_recv_put_signal(mv, rma, &c1);
        mv_send_put_signal(mv, src, size, 1, remote_rma, &c2);

        while (!mv_test(&c2))
          mv_progress(mv);

        while (!mv_test(&c1))
          mv_progress(mv);

        if (i == 0)
          for (int j = 0; j < size; j++)
            assert(buf[j] == 'A');
      } else {
        while (!mv_test(&c2))
          mv_progress(mv);
        if (i == 0)
          for (int j = 0; j < size; j++)
            assert(buf[j] == 'A');

        mv_recv_put_signal(mv, rma, &c2);
        mv_send_put_signal(mv, src, size, 0, remote_rma, &c1);
        while (!mv_test(&c1))
          mv_progress(mv);
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      t1 = MPI_Wtime() - t1;
      printf("%d \t %.5f \n", size, t1 * 1e6 / TOTAL / 2);
    }
  }
  mv_close(mv);
}

void main_task(intptr_t a)
{
}
