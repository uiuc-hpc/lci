#include <iostream>
#define LCIT_BENCH
#include "lcit.h"

using namespace lcit;

void test(Context ctx) {
  int rank = LCI_RANK;
  int tag = 245;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;

  if (rank % 2 == 0) {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      threadBarrier(ctx);
      fflush(stdout);

      double time = RUN_VARY_MSG(ctx, [&]() {
        std::vector<LCI_comp_t> comps;
        // send
        for (int j = 0; j < ctx.config.send_window; ++j) {
          LCI_comp_t comp = postSend(ctx, peer_rank, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitSend(ctx, comp);
        }
        // recv
        comps.clear();
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          LCI_comp_t comp = postRecv(ctx, peer_rank, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitRecv(ctx, comp);
        }
      });
      if (TRD_RANK_ME == 0 && LCI_RANK == 0) {
        double bw = size * ctx.config.send_window / time / 1e6;
        printf("%d %.2f %.2f\n", size, time * 1e6, bw);
      }
    }
  } else {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size; size <<= 1) {
      threadBarrier(ctx);
      RUN_VARY_MSG(ctx, [&]() {
        std::vector<LCI_comp_t> comps;
        // recv
        for (int j = 0; j < ctx.config.send_window; ++j) {
          LCI_comp_t comp = postRecv(ctx, peer_rank, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitRecv(ctx, comp);
        }
        // send
        comps.clear();
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          LCI_comp_t comp = postSend(ctx, peer_rank, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitSend(ctx, comp);
        }
      });
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
