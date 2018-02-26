#include "lc.h"
#include "comm_exp.h"
#include "helper_fult.h"

#include <assert.h>
#include <string.h>

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1<<22)

lch* mv;
volatile int done = 0;

void* wait_put(void* arg)
{
  lc_req* req = (lc_req*) arg;
  lc_sync sync = LC_SYNC_INITIALIZER;
  lc_wait_post(&sync, req);
  done = 1;
  return 0;
}

int main(int argc, char** args)
{
  lc_open(&mv);
  // Need to worker, the main one polling network.
  MPI_Start_worker(2);

  char* buf = (char*) malloc(MAX_MSG_SIZE);
  lc_addr rma, rma_remote;
  lc_rma_init(mv, buf, MAX_MSG_SIZE, &rma);
  int rank = lc_id(mv);
  fthread_t thread;
  lc_req req2;

  // Exchange the lc_addr.
  if (rank == 0) {
    lc_req s;
    lc_send_tag(mv, &rma, sizeof(lc_addr), 1, 0, &s);
    lc_wait_poll(mv, &s);

    lc_recv_tag(mv, &rma_remote, sizeof(lc_addr), 1, 0, &s);
    lc_wait_poll(mv, &s);
  } else {
    // Make sure the req is attach to the rma, before the put occur.
    lc_recv_put(mv, &rma, &req2);
    MPI_spawn(1, wait_put, &req2, &thread);

    lc_req s;
    lc_recv_tag(mv, &rma_remote, sizeof(lc_addr), 0, 0, &s);
    lc_wait_poll(mv, &s);

    lc_send_tag(mv, &rma, sizeof(lc_addr), 0, 0, &s);
    lc_wait_poll(mv, &s);
  }

  printf("Done exchange address\n");

  void* src = malloc(MAX_MSG_SIZE);
  memset(src, 'A', MAX_MSG_SIZE);

  if (rank == 1) {
    // polling until it has received.
    while (!done) {
      lc_progress(mv);
    }
    MPI_join(&thread);
  } else {
    lc_req req;
    lc_send_put(mv, src, 64, &rma_remote, &req);
    lc_wait_poll(mv, &req);
  }

  printf("%d done\n", rank);

  lc_rma_fini(mv, &rma);
  lc_close(mv);
  MPI_Stop_worker();
}
