#ifndef LCI_LCIT_H
#define LCI_LCIT_H

#ifndef NDEBUG
#define LCI_DEBUG
#endif

#include <iostream>
#include <functional>
#include <getopt.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <atomic>
#include <type_traits>
#include <cstring>
#include <sys/time.h>
#include "lci.h"
#include "lcit_threadbarrier.h"

namespace lcit
{
const size_t CACHESIZE_L1 = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
const size_t PAGESIZE = sysconf(_SC_PAGESIZE);
const size_t NPROCESSORS = sysconf(_SC_NPROCESSORS_ONLN);
const uint64_t USER_CONTEXT = 345;
__thread int TRD_RANK_ME = 0;
__thread bool to_progress = true;
alignas(64) volatile bool progress_exit = false;

enum Op {
  LCIT_OP_2SIDED_S = 0,
  LCIT_OP_2SIDED_M,
  LCIT_OP_2SIDED_L,
  LCIT_OP_1SIDED_S,
  LCIT_OP_1SIDED_M,
  LCIT_OP_1SIDED_L,
};

struct Config {
  Op op = LCIT_OP_2SIDED_M;
  bool send_dyn = false;
  bool recv_dyn = false;
  bool send_reg = false;
  bool recv_reg = false;
  LCI_match_t match_type = LCI_MATCH_RANKTAG;
  LCI_comp_type_t send_comp_type = LCI_COMPLETION_SYNC;
  LCI_comp_type_t recv_comp_type = LCI_COMPLETION_SYNC;
  LCI_handler_t comp_handler = NULL;
  int nthreads = 1;
  bool thread_pin = false;
  size_t min_msg_size = 8;
  size_t max_msg_size = LCI_MEDIUM_SIZE;
  int send_window = 1;
  int recv_window = 1;
  bool touch_data = false;
  size_t nsteps = 1000;
};

void checkConfig(Config& config)
{
  if (config.op >= LCIT_OP_1SIDED_S &&
      config.recv_comp_type != LCI_COMPLETION_QUEUE) {
    LCT_Warn(
        LCT_log_ctx_default,
        "Currently one-sided communication only support "
        "--recv-comp-type=queue. Change the receive completion to queue \n");
    config.recv_comp_type = LCI_COMPLETION_QUEUE;
  }
  if (config.op < LCIT_OP_1SIDED_S &&
      (config.send_comp_type == LCI_COMPLETION_QUEUE ||
       config.recv_comp_type == LCI_COMPLETION_QUEUE)) {
    LCT_Warn(LCT_log_ctx_default,
             "Completion queue does not work well with 2-sided ping-pong tests."
             "You may encounter deadlock!\n");
  }
}

void printConfig(const Config& config)
{
  printf(
      "Configuration:\n"
      "op: %d\n"
      "send_dyn: %d\n"
      "recv_dyn: %d\n"
      "send_reg: %d\n"
      "recv_reg: %d\n"
      "match_type: %d\n"
      "send_comp_type: %d\n"
      "recv_comp_type: %d\n"
      "nthreads: %d\n"
      "thread_pin: %d\n"
      "min_msg_size: %lu\n"
      "max_msg_size: %lu\n"
      "send_window: %d\n"
      "recv_window: %d\n"
      "touch_data: %d\n"
      "steps: %lu\n",
      config.op, config.send_dyn, config.recv_dyn, config.send_reg,
      config.recv_reg, config.match_type, config.send_comp_type,
      config.recv_comp_type, config.nthreads, config.thread_pin,
      config.min_msg_size, config.max_msg_size, config.send_window,
      config.recv_window, config.touch_data, config.nsteps);
};

enum LongFlags {
  OP = 0,
  SEND_DYN,
  RECV_DYN,
  SEND_REG,
  RECV_REG,
  MATCH_TYPE,
  SEND_COMP_TYPE,
  RECV_COMP_TYPE,
  NTHREADS,
  THREAD_PIN,
  MIN_MSG_SIZE = 10,
  MAX_MSG_SIZE,
  SEND_WINDOW,
  RECV_WINDOW,
  TOUCH_DATA,
  NSTEPS,
};

void init() { LCI_initialize(); }

void fina() { LCI_finalize(); }

Config parseArgs(int argc, char** argv)
{
  Config config;
  int opt;
  int long_flag;
  opterr = 0;

  struct option long_options[] = {
      {"op", required_argument, &long_flag, OP},
      {"send-dyn", required_argument, &long_flag, SEND_DYN},
      {"recv-dyn", required_argument, &long_flag, RECV_DYN},
      {"send-reg", required_argument, &long_flag, SEND_REG},
      {"recv-reg", required_argument, &long_flag, RECV_REG},
      {"match-type", required_argument, &long_flag, MATCH_TYPE},
      {"send-comp-type", required_argument, &long_flag, SEND_COMP_TYPE},
      {"recv-comp-type", required_argument, &long_flag, RECV_COMP_TYPE},
      {"nthreads", required_argument, &long_flag, NTHREADS},
      {"thread-pin", required_argument, &long_flag, THREAD_PIN},
      {"min-msg-size", required_argument, &long_flag, MIN_MSG_SIZE},
      {"max-msg-size", required_argument, &long_flag, MAX_MSG_SIZE},
      {"send-window", required_argument, &long_flag, SEND_WINDOW},
      {"recv-window", required_argument, &long_flag, RECV_WINDOW},
      {"touch-data", required_argument, &long_flag, TOUCH_DATA},
      {"nsteps", required_argument, &long_flag, NSTEPS},
      {0, 0, 0, 0}};
  while ((opt = getopt_long(argc, argv, "t:", long_options, NULL)) != -1) {
    switch (opt) {
      case 0:
        // long options
        switch (long_flag) {
          case OP:
            if (strcmp(optarg, "2s") == 0)
              config.op = LCIT_OP_2SIDED_S;
            else if (strcmp(optarg, "2m") == 0)
              config.op = LCIT_OP_2SIDED_M;
            else if (strcmp(optarg, "2l") == 0)
              config.op = LCIT_OP_2SIDED_L;
            else if (strcmp(optarg, "1s") == 0)
              config.op = LCIT_OP_1SIDED_S;
            else if (strcmp(optarg, "1m") == 0)
              config.op = LCIT_OP_1SIDED_M;
            else if (strcmp(optarg, "1l") == 0)
              config.op = LCIT_OP_1SIDED_L;
            else {
              fprintf(stderr, "Unknown long option (--op %s)\n", optarg);
              abort();
            }
            break;
          case SEND_DYN:
            config.send_dyn = atoi(optarg);
            break;
          case RECV_DYN:
            config.recv_dyn = atoi(optarg);
            break;
          case SEND_REG:
            config.send_reg = atoi(optarg);
            break;
          case RECV_REG:
            config.recv_reg = atoi(optarg);
            break;
          case MATCH_TYPE:
            if (strcmp(optarg, "ranktag") == 0)
              config.match_type = LCI_MATCH_RANKTAG;
            else if (strcmp(optarg, "tag") == 0)
              config.match_type = LCI_MATCH_TAG;
            else {
              fprintf(stderr, "Unknown long option (--match-type %s)\n",
                      optarg);
              abort();
            }
            break;
          case SEND_COMP_TYPE:
            if (strcmp(optarg, "sync") == 0)
              config.send_comp_type = LCI_COMPLETION_SYNC;
            else if (strcmp(optarg, "queue") == 0)
              config.send_comp_type = LCI_COMPLETION_QUEUE;
            else if (strcmp(optarg, "handler") == 0)
              config.send_comp_type = LCI_COMPLETION_HANDLER;
            else {
              fprintf(stderr, "Unknown long option (--send-comp-type %s)\n",
                      optarg);
              abort();
            }
            break;
          case RECV_COMP_TYPE:
            if (strcmp(optarg, "sync") == 0)
              config.recv_comp_type = LCI_COMPLETION_SYNC;
            else if (strcmp(optarg, "queue") == 0)
              config.recv_comp_type = LCI_COMPLETION_QUEUE;
            else if (strcmp(optarg, "handler") == 0)
              config.recv_comp_type = LCI_COMPLETION_HANDLER;
            else {
              fprintf(stderr, "Unknown long option (--send-comp-type %s)\n",
                      optarg);
              abort();
            }
            break;
          case NTHREADS:
            config.nthreads = atoi(optarg);
            break;
          case THREAD_PIN:
            config.thread_pin = atoi(optarg);
            break;
          case MIN_MSG_SIZE:
            config.min_msg_size = atoi(optarg);
            break;
          case MAX_MSG_SIZE:
            config.max_msg_size = atoi(optarg);
            break;
          case SEND_WINDOW:
            config.send_window = atoi(optarg);
            break;
          case RECV_WINDOW:
            config.recv_window = atoi(optarg);
            break;
          case TOUCH_DATA:
            config.touch_data = atoi(optarg);
            break;
          case NSTEPS:
            config.nsteps = atoi(optarg);
            break;
          default:
            fprintf(stderr, "Unknown long flag %d\n", long_flag);
            break;
        }
        break;
      case 't':
        config.touch_data = atoi(optarg);
        break;
      default:
        break;
    }
  }
  checkConfig(config);
  return config;
}

struct Context {
  Config config;
  LCI_device_t device;
  LCI_endpoint_t ep;
  LCI_comp_t send_comp;
  LCI_comp_t recv_comp;
  LCI_data_t send_data;
  LCI_data_t recv_data;
  ThreadBarrier* threadBarrier = nullptr;
};

LCI_comp_t initComp(Context ctx, LCI_comp_type_t comp_type)
{
  LCI_comp_t comp;
  if (comp_type == LCI_COMPLETION_QUEUE) {
    LCI_queue_create(ctx.device, &comp);
  } else if (comp_type == LCI_COMPLETION_SYNC) {
    // do nothing
  } else if (comp_type == LCI_COMPLETION_HANDLER) {
    LCI_handler_create(ctx.device, ctx.config.comp_handler, &comp);
  } else {
    fprintf(stderr, "Unknown completion type %d\n", comp_type);
    abort();
  }
  return comp;
}

void freeComp(LCI_comp_type_t comp_type, LCI_comp_t* comp)
{
  if (comp_type == LCI_COMPLETION_QUEUE) {
    LCI_queue_free(comp);
  } else if (comp_type == LCI_COMPLETION_SYNC) {
    // do nothing
  } else if (comp_type == LCI_COMPLETION_HANDLER) {
  } else {
    fprintf(stderr, "Unknown completion type %d\n", comp_type);
    abort();
  }
}

LCI_request_t waitComp(Context& ctx, LCI_comp_t comp, LCI_comp_type_t comp_type)
{
  LCI_request_t request;
  if (comp_type == LCI_COMPLETION_QUEUE) {
    while (LCI_queue_pop(comp, &request) == LCI_ERR_RETRY)
      if (to_progress) LCI_progress(ctx.device);
  } else if (comp_type == LCI_COMPLETION_SYNC) {
    while (LCI_sync_test(comp, &request) == LCI_ERR_RETRY)
      if (to_progress) LCI_progress(ctx.device);
  } else if (comp_type == LCI_COMPLETION_HANDLER) {
  } else {
    fprintf(stderr, "Unknown completion type %d\n", comp_type);
    abort();
  }
  return request;
}

void initData(Context& ctx)
{
  switch (ctx.config.op) {
    case LCIT_OP_2SIDED_S:
    case LCIT_OP_1SIDED_S:
      break;
    case LCIT_OP_2SIDED_M:
    case LCIT_OP_1SIDED_M:
      if (ctx.config.send_dyn) {
        while (LCI_mbuffer_alloc(ctx.device, &ctx.send_data.mbuffer) ==
               LCI_ERR_RETRY)
          if (to_progress) LCI_progress(ctx.device);
      } else {
        posix_memalign(&ctx.send_data.mbuffer.address, PAGESIZE,
                       ctx.config.max_msg_size);
        ctx.send_data.mbuffer.length = ctx.config.max_msg_size;
      }
      if (ctx.config.recv_dyn) {
        // do nothing
      } else {
        posix_memalign(&ctx.recv_data.mbuffer.address, PAGESIZE,
                       ctx.config.max_msg_size);
        ctx.recv_data.mbuffer.length = ctx.config.max_msg_size;
      }
      break;
    case LCIT_OP_2SIDED_L:
    case LCIT_OP_1SIDED_L:
      if (ctx.config.send_reg) {
        LCI_lbuffer_memalign(ctx.device, ctx.config.max_msg_size, PAGESIZE,
                             &ctx.send_data.lbuffer);
      } else {
        posix_memalign(&ctx.send_data.lbuffer.address, PAGESIZE,
                       ctx.config.max_msg_size);
        ctx.send_data.lbuffer.length = ctx.config.max_msg_size;
        ctx.send_data.lbuffer.segment = LCI_SEGMENT_ALL;
      }
      if (ctx.config.recv_dyn) {
        ctx.recv_data.lbuffer.address = NULL;
      } else if (ctx.config.recv_reg) {
        LCI_lbuffer_memalign(ctx.device, ctx.config.max_msg_size, PAGESIZE,
                             &ctx.recv_data.lbuffer);
      } else {
        posix_memalign(&ctx.recv_data.lbuffer.address, PAGESIZE,
                       ctx.config.max_msg_size);
        ctx.recv_data.lbuffer.length = ctx.config.max_msg_size;
        ctx.recv_data.lbuffer.segment = LCI_SEGMENT_ALL;
      }
      break;
  }
}

void freeData(Context& ctx)
{
  switch (ctx.config.op) {
    case LCIT_OP_2SIDED_S:
    case LCIT_OP_1SIDED_S:
      break;
    case LCIT_OP_2SIDED_M:
    case LCIT_OP_1SIDED_M:
      if (ctx.config.send_dyn) {
        LCI_mbuffer_free(ctx.send_data.mbuffer);
      } else {
        free(ctx.send_data.mbuffer.address);
        ctx.send_data.mbuffer.length = 0;
      }
      if (ctx.config.recv_dyn) {
        // do nothing
      } else {
        free(ctx.recv_data.mbuffer.address);
        ctx.recv_data.mbuffer.length = 0;
      }
      break;
    case LCIT_OP_2SIDED_L:
    case LCIT_OP_1SIDED_L:
      if (ctx.config.send_reg) {
        LCI_lbuffer_free(ctx.send_data.lbuffer);
      } else {
        free(ctx.send_data.lbuffer.address);
        ctx.send_data.lbuffer.length = 0;
      }
      if (ctx.config.recv_reg) {
        LCI_lbuffer_free(ctx.recv_data.lbuffer);
      } else {
        free(ctx.recv_data.lbuffer.address);
        ctx.recv_data.lbuffer.length = 0;
      }
      break;
  }
}

Context initCtx(Config config)
{
  Context ctx;
  ctx.config = config;
  ctx.device = LCI_UR_DEVICE;
  ctx.send_comp = initComp(ctx, config.send_comp_type);
  ctx.recv_comp = initComp(ctx, config.recv_comp_type);

  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_match_type(plist, config.match_type);
  LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, config.send_comp_type);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, config.recv_comp_type);
  LCI_plist_set_default_comp(plist, ctx.recv_comp);
  LCI_endpoint_init(&ctx.ep, ctx.device, plist);
  LCI_plist_free(&plist);

  initData(ctx);
  if (config.nthreads > 1)
    ctx.threadBarrier = new ThreadBarrier(config.nthreads - 1);

  return ctx;
}

void freeCtx(Context& ctx)
{
  if (ctx.config.nthreads > 1) delete ctx.threadBarrier;
  freeData(ctx);
  freeComp(ctx.config.send_comp_type, &ctx.send_comp);
  freeComp(ctx.config.recv_comp_type, &ctx.recv_comp);
  LCI_endpoint_free(&ctx.ep);
}

void threadBarrier(Context& ctx)
{
  if (ctx.config.nthreads > 1) {
    if (to_progress)
      ctx.threadBarrier->wait(LCI_progress, ctx.device);
    else
      ctx.threadBarrier->wait();
  }
}

LCI_comp_t postSend(Context& ctx, int rank, size_t size, LCI_tag_t tag)
{
  LCT_DBG_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "lcit",
              "%d/%d: postSend rank %d size %lu tag %d\n", LCI_RANK,
              TRD_RANK_ME, rank, size, tag);
  LCI_comp_t comp;
  if (ctx.config.send_comp_type == LCI_COMPLETION_SYNC) {
    LCI_sync_create(ctx.device, 1, &comp);
  } else {
    comp = ctx.send_comp;
  }
  switch (ctx.config.op) {
    case LCIT_OP_2SIDED_S:
      while (LCI_sends(ctx.ep, ctx.send_data.immediate, rank, tag) ==
             LCI_ERR_RETRY)
        if (to_progress) LCI_progress(ctx.device);
      break;
    case LCIT_OP_2SIDED_M: {
      if (ctx.config.send_dyn) {
        while (LCI_mbuffer_alloc(ctx.device, &ctx.send_data.mbuffer) ==
               LCI_ERR_RETRY)
          if (to_progress) LCI_progress(ctx.device);
      }
      LCI_mbuffer_t send_buffer = ctx.send_data.mbuffer;
      send_buffer.length = size;
      if (ctx.config.send_dyn) {
        while (LCI_sendmn(ctx.ep, send_buffer, rank, tag) == LCI_ERR_RETRY)
          if (to_progress) LCI_progress(ctx.device);
      } else {
        while (LCI_sendm(ctx.ep, send_buffer, rank, tag) == LCI_ERR_RETRY)
          if (to_progress) LCI_progress(ctx.device);
      }
      break;
    }
    case LCIT_OP_2SIDED_L: {
      LCI_lbuffer_t send_buffer = ctx.send_data.lbuffer;
      send_buffer.length = size;
      while (LCI_sendl(ctx.ep, send_buffer, rank, tag, comp,
                       (void*)USER_CONTEXT) == LCI_ERR_RETRY)
        if (to_progress) LCI_progress(ctx.device);
      break;
    }
    case LCIT_OP_1SIDED_S:
      while (LCI_puts(ctx.ep, ctx.send_data.immediate, rank, tag,
                      LCI_DEFAULT_COMP_REMOTE) == LCI_ERR_RETRY)
        if (to_progress) LCI_progress(ctx.device);
      break;
    case LCIT_OP_1SIDED_M: {
      if (ctx.config.send_dyn) {
        while (LCI_mbuffer_alloc(ctx.device, &ctx.send_data.mbuffer) ==
               LCI_ERR_RETRY)
          if (to_progress) LCI_progress(ctx.device);
      }
      LCI_mbuffer_t send_buffer = ctx.send_data.mbuffer;
      send_buffer.length = size;
      if (ctx.config.send_dyn) {
        while (LCI_putmna(ctx.ep, send_buffer, rank, tag,
                          LCI_DEFAULT_COMP_REMOTE) == LCI_ERR_RETRY)
          if (to_progress) LCI_progress(ctx.device);
      } else {
        while (LCI_putma(ctx.ep, send_buffer, rank, tag,
                         LCI_DEFAULT_COMP_REMOTE) == LCI_ERR_RETRY)
          if (to_progress) LCI_progress(ctx.device);
      }
      break;
    }
    case LCIT_OP_1SIDED_L: {
      LCI_lbuffer_t send_buffer = ctx.send_data.lbuffer;
      send_buffer.length = size;
      while (LCI_putla(ctx.ep, send_buffer, comp, rank, tag,
                       LCI_DEFAULT_COMP_REMOTE,
                       (void*)USER_CONTEXT) == LCI_ERR_RETRY)
        if (to_progress) LCI_progress(ctx.device);
      break;
    }
  }
  return comp;
}

void waitSend(Context& ctx, LCI_comp_t comp)
{
  LCT_DBG_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "lcit", "%d/%d: waitSend\n",
              LCI_RANK, TRD_RANK_ME);
  switch (ctx.config.op) {
    case LCIT_OP_2SIDED_S:
    case LCIT_OP_2SIDED_M:
    case LCIT_OP_1SIDED_S:
    case LCIT_OP_1SIDED_M:
      break;
    case LCIT_OP_2SIDED_L:
    case LCIT_OP_1SIDED_L:
      LCI_request_t request = waitComp(ctx, comp, ctx.config.send_comp_type);
      LCT_Assert(LCT_log_ctx_default, request.flag == LCI_OK,
                 "flag is wrong\n");
      LCT_Assert(LCT_log_ctx_default, request.type == LCI_LONG,
                 "type is wrong\n");
      LCT_Assert(LCT_log_ctx_default,
                 request.data.lbuffer.address == ctx.send_data.lbuffer.address,
                 "address is wrong\n");
      LCT_Assert(LCT_log_ctx_default,
                 request.data.lbuffer.segment == ctx.send_data.lbuffer.segment,
                 "segment is wrong\n");
      LCT_Assert(LCT_log_ctx_default,
                 (uint64_t)request.user_context == USER_CONTEXT,
                 "user_context is wrong\n");
      break;
  }
}

LCI_comp_t postRecv(Context& ctx, int rank, size_t size, LCI_tag_t tag)
{
  LCT_DBG_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "lcit",
              "%d/%d: postRecv rank %d size %lu tag %d\n", LCI_RANK,
              TRD_RANK_ME, rank, size, tag);
  LCI_comp_t comp;
  if (ctx.config.recv_comp_type == LCI_COMPLETION_SYNC) {
    LCI_sync_create(ctx.device, 1, &comp);
  } else {
    comp = ctx.recv_comp;
  }
  switch (ctx.config.op) {
    case LCIT_OP_2SIDED_S:
      LCI_recvs(ctx.ep, rank, tag, comp, (void*)USER_CONTEXT);
      break;
    case LCIT_OP_2SIDED_M: {
      LCI_mbuffer_t recv_buffer = ctx.send_data.mbuffer;
      recv_buffer.length = size;
      if (ctx.config.recv_dyn) {
        LCI_recvmn(ctx.ep, rank, tag, comp, (void*)USER_CONTEXT);
      } else {
        LCI_recvm(ctx.ep, recv_buffer, rank, tag, comp, (void*)USER_CONTEXT);
      }
      break;
    }
    case LCIT_OP_2SIDED_L: {
      LCI_lbuffer_t recv_buffer = ctx.recv_data.lbuffer;
      recv_buffer.length = size;
      LCI_recvl(ctx.ep, recv_buffer, rank, tag, comp, (void*)USER_CONTEXT);
      break;
    }
    case LCIT_OP_1SIDED_S:
    case LCIT_OP_1SIDED_M:
    case LCIT_OP_1SIDED_L:
      break;
  }
  return comp;
}

void waitRecv(Context& ctx, LCI_comp_t comp)
{
  LCT_DBG_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "lcit", "%d/%d: waitRecv\n",
              LCI_RANK, TRD_RANK_ME);
  LCI_request_t request = waitComp(ctx, comp, ctx.config.recv_comp_type);
  LCT_Assert(LCT_log_ctx_default, request.flag == LCI_OK, "flag is wrong\n");
  if (ctx.config.op == LCIT_OP_2SIDED_L || ctx.config.op == LCIT_OP_2SIDED_M ||
      ctx.config.op == LCIT_OP_2SIDED_S)
    LCT_Assert(LCT_log_ctx_default,
               (uint64_t)request.user_context == USER_CONTEXT,
               "user_context is wrong\n");
  switch (ctx.config.op) {
    case LCIT_OP_2SIDED_S:
    case LCIT_OP_1SIDED_S:
      LCT_Assert(LCT_log_ctx_default, request.type == LCI_IMMEDIATE,
                 "type is wrong\n");
      break;
    case LCIT_OP_2SIDED_M:
      LCT_Assert(LCT_log_ctx_default, request.type == LCI_MEDIUM,
                 "type is wrong\n");
      if (ctx.config.recv_dyn) {
        LCI_mbuffer_free(request.data.mbuffer);
      }
      break;
    case LCIT_OP_1SIDED_M:
      LCT_Assert(LCT_log_ctx_default, request.type == LCI_MEDIUM,
                 "type is wrong\n");
      LCI_mbuffer_free(request.data.mbuffer);
      break;
    case LCIT_OP_2SIDED_L:
      LCT_Assert(LCT_log_ctx_default, request.type == LCI_LONG,
                 "type is wrong\n");
      if (ctx.config.recv_dyn) {
        LCI_lbuffer_free(request.data.lbuffer);
      }
      break;
    case LCIT_OP_1SIDED_L:
      LCT_Assert(LCT_log_ctx_default, request.type == LCI_LONG,
                 "type is wrong\n");
      LCI_lbuffer_free(request.data.lbuffer);
      break;
  }
}

template <typename Fn, typename... Args>
void worker_handler(Fn&& fn, int id, Args&&... args)
{
  TRD_RANK_ME = id;
  to_progress = false;
  fn(std::forward<Args>(args)...);
  //  std::invoke(std::forward<Fn>(fn),
  //              std::forward<Args>(args)...);
}

// progress thread
void progress_handler(LCI_device_t device)
{
  to_progress = true;
  while (!progress_exit) {
    LCI_progress(device);
  }
}

void set_affinity(pthread_t pthread_handler, size_t target)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(target, &cpuset);
  int rv = pthread_setaffinity_np(pthread_handler, sizeof(cpuset), &cpuset);
  if (rv != 0) {
    fprintf(stderr, "ERROR %d thread affinity didn't work.\n", rv);
    exit(1);
  }
}

template <typename Fn, typename... Args>
void run(Context& ctx, Fn&& fn, Args&&... args)
{
  using fn_t =
      decltype(+std::declval<typename std::remove_reference<Fn>::type>());
  std::vector<std::thread> worker_pool;
  std::vector<std::thread> progress_pool;

  if (ctx.config.nthreads > 1) {
    // Multithreaded version
    // initialize progress thread
    progress_exit = false;
    std::thread t(progress_handler, ctx.device);
    if (ctx.config.thread_pin) set_affinity(t.native_handle(), 0);
    progress_pool.push_back(std::move(t));

    // initialize worker threads
    for (size_t i = 0; i < ctx.config.nthreads - 1; ++i) {
      std::thread t(
          worker_handler<fn_t, typename std::remove_reference<Args>::type...>,
          +fn, i, args...);
      if (ctx.config.thread_pin)
        set_affinity(t.native_handle(), (i + 1) % NPROCESSORS);
      worker_pool.push_back(std::move(t));
    }

    // wait for workers to finish
    for (size_t i = 0; i < ctx.config.nthreads - 1; ++i) {
      worker_pool[i].join();
    }

    // wait for progress threads to finish
    progress_exit = true;
    progress_pool[0].join();
  } else {
    // Singlethreaded version
    TRD_RANK_ME = 0;
    to_progress = true;
    fn(std::forward<Args>(args)...);
  }
}

static inline double wtime()
{
  struct timespec t1;
  int ret = clock_gettime(CLOCK_MONOTONIC, &t1);
  if (ret != 0) {
    fprintf(stderr, "Cannot get time!\n");
    abort();
  }
  return t1.tv_sec + t1.tv_nsec / 1e9;
}

template <typename FUNC>
static inline double RUN_VARY_MSG(Context& ctx, FUNC&& f)
{
  int loop = ctx.config.nsteps;
  int skip = loop / 10;
  for (int i = 0; i < skip; ++i) {
    f();
  }
  threadBarrier(ctx);
  double t = wtime();
  for (int i = 0; i < loop; ++i) {
    f();
  }
  threadBarrier(ctx);
  t = wtime() - t;

  return t / loop;
}

}  // namespace lcit

#endif  // LCI_LCIT_H
