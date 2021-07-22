#include <iostream>
#include "lcit.h"

using namespace lcit;

void test(Context ctx) {
  int rank = LCI_RANK;
  int tag = 245 + TRD_RANK_ME;

  if (rank == 0) {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      printf("Testing message size %d...\n", size);
      fflush(stdout);

      for (int i = 0; i < ctx.config.nsteps; ++i) {
        std::vector<LCI_comp_t> comps;
        // recv
        threadBarrier(ctx);
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          for (int target_rank = 1; target_rank < LCI_NUM_PROCESSES; ++target_rank) {
            LCI_comp_t comp = postRecv(ctx, target_rank, size, tag);
            comps.push_back(comp);
          }
        }
        for (auto comp : comps) {
          waitRecv(ctx, comp);
        }
        // send
        threadBarrier(ctx);
        comps.clear();
        for (int j = 0; j < ctx.config.send_window; ++j) {
          for (int target_rank = 1; target_rank < LCI_NUM_PROCESSES; ++target_rank) {
            LCI_comp_t comp = postSend(ctx, target_rank, size, tag);
            comps.push_back(comp);
          }
        }
        for (auto comp : comps) {
          waitSend(ctx, comp);
        }
      }
    }
  } else {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      for (int i = 0; i < ctx.config.nsteps; ++i) {
        std::vector<LCI_comp_t> comps;
        // send
        threadBarrier(ctx);
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          LCI_comp_t comp = postSend(ctx, 0, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitSend(ctx, comp);
        }
        // recv
        threadBarrier(ctx);
        comps.clear();
        for (int j = 0; j < ctx.config.send_window; ++j) {
          LCI_comp_t comp = postRecv(ctx, 0, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitRecv(ctx, comp);
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
