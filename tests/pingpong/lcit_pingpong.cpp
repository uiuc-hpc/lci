#include "lci.h"
#include <iostream>
#include "lcit.h"
#include "comm_exp.h"

using namespace lcit;

void test(Context ctx) {
  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;

  if (rank % 2 == 0) {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      printf("Testing message size %d...\n", size);
      fflush(stdout);

      for (int i = 0; i < ctx.config.nsteps; ++i) {
        for (int j = 0; j < ctx.config.send_window; ++j) {
          postSend(ctx, peer_rank, size);
          waitSend(ctx);
        }
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          postRecv(ctx, peer_rank, size);
          waitRecv(ctx);
        }
      }
    }
  } else {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      for (int i = 0; i < ctx.config.nsteps; ++i) {
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          postRecv(ctx, peer_rank, size);
          waitRecv(ctx);
        }
        for (int j = 0; j < ctx.config.send_window; ++j) {
          postSend(ctx, peer_rank, size);
          waitSend(ctx);
        }
      }
    }
  }
}

int main(int argc, char** args) {
  LCI_initialize();
  Config config = parseArgs(argc, args);
  Context ctx = initCtx(config);

  run(ctx, test, ctx);

  freeCtx(ctx);
  LCI_finalize();
  return 0;
}
