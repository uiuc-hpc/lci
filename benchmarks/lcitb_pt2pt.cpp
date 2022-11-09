#include <iostream>
#define LCIT_BENCH
#include "lcit.h"

using namespace lcit;

void test(Context ctx)
{
  int tag = 245 + TRD_RANK_ME;
  //  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) %
  //  LCI_NUM_PROCESSES; // 0 <-> 1, 2 <-> 3
  int peer_rank = (LCI_RANK + LCI_NUM_PROCESSES / 2) %
                  LCI_NUM_PROCESSES;  // 0 <-> 2 1 <-> 3

  if (LCI_RANK < LCI_NUM_PROCESSES / 2) {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size;
         size <<= 1) {
      threadBarrier(ctx);

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
        int worker_thread_num =
            (ctx.config.nthreads == 1 ? 1 : ctx.config.nthreads - 1) *
            LCI_NUM_PROCESSES / 2;
        double latency_us = time * 1e6;
        double msg_rate_mps =
            ctx.config.send_window / latency_us * worker_thread_num;
        double bw_mbps = size * msg_rate_mps;
        printf("%d %.2f %.2f %.2f\n", size, latency_us, msg_rate_mps, bw_mbps);
      }
    }
  } else {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size;
         size <<= 1) {
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

int main(int argc, char** args)
{
  LCI_initialize();
  Config config = parseArgs(argc, args);
  Context ctx = initCtx(config);
  if (LCI_RANK == 0) printConfig(config);

  run(ctx, test, ctx);

  freeCtx(ctx);
  LCI_finalize();
  return 0;
}
