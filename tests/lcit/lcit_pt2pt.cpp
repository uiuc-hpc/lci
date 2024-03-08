#include <iostream>
#include "lcit.h"

using namespace lcit;

void test(Context ctx)
{
  Data data = initData(ctx);
  int rank = LCI_RANK;
  int tag = 245 + TRD_RANK_ME;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;

  if (rank % 2 == 0) {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size;
         size <<= 1) {
      threadBarrier(ctx);
      printf("Testing message size %d...\n", size);
      fflush(stdout);

      for (int i = 0; i < ctx.config.nsteps; ++i) {
        std::vector<LCI_comp_t> comps;
        // send
        for (int j = 0; j < ctx.config.send_window; ++j) {
          LCI_comp_t comp = postSend(ctx, peer_rank, data, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitSend(ctx, data, comp);
        }
        // recv
        comps.clear();
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          LCI_comp_t comp = postRecv(ctx, peer_rank, data, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitRecv(ctx, data, comp);
        }
      }
    }
  } else {
    for (int size = ctx.config.min_msg_size; size <= ctx.config.max_msg_size;
         size <<= 1) {
      threadBarrier(ctx);
      for (int i = 0; i < ctx.config.nsteps; ++i) {
        std::vector<LCI_comp_t> comps;
        // recv
        for (int j = 0; j < ctx.config.send_window; ++j) {
          LCI_comp_t comp = postRecv(ctx, peer_rank, data, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitRecv(ctx, data, comp);
        }
        // send
        comps.clear();
        for (int j = 0; j < ctx.config.recv_window; ++j) {
          LCI_comp_t comp = postSend(ctx, peer_rank, data, size, tag);
          comps.push_back(comp);
        }
        for (auto comp : comps) {
          waitSend(ctx, data, comp);
        }
      }
    }
  }
  freeData(ctx, data);
}

int main(int argc, char** args)
{
  init();
  Config config = parseArgs(argc, args);
  Context ctx = initCtx(config);

  run(ctx, test, ctx);

  freeCtx(ctx);
  fina();
  return 0;
}
