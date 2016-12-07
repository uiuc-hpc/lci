#ifndef MPIV_MV_H_
#define MPIV_MV_H_

#include "config.h"
#include "macro.h"
#include "ult.h"
#include <stdint.h>
#include <stdlib.h>

/*! Init context */
struct mv_struct;
typedef struct mv_struct mv_engine;
void mv_open(int* argc, char*** args, size_t heap_size, mv_engine**);
void mv_close(mv_engine*);

/*! Memory function */
void* mv_heap_ptr(mv_engine*);
void mv_set_num_worker(mv_engine*, int number);

/*! Two-sided communication function */
typedef void (*mv_am_func_t)();
struct mv_ctx;
typedef struct mv_ctx mv_ctx;

void mv_send_eager(mv_engine* mv, mv_ctx* ctx);
void mv_send_rdz(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);

void mv_recv_eager(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);
void mv_recv_rdz(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);

/*! One-sided communication function */
void mv_am_eager(mv_engine*, int dst_rank, void* src, int size, uint32_t fid);
void mv_put(mv_engine*, int dst_rank, void* dst, void* src, int size,
            uint32_t fid);
// void mv_am_rdz(mv_engine*, int dst_rank, void* src, int size, uint32_t fid);
uint8_t mv_am_register(mv_engine* mv, mv_am_func_t f);

// void mv_get(mv_engine*, int dst_rank, void* src, void* dst, size_t size,
// mv_sync* sync);

struct mv_packet;
typedef struct mv_packet mv_packet;
struct mv_pool;
typedef struct mv_pool mv_pool;

#if defined(MV_USE_SERVER_IBV)
typedef struct ibv_server mv_server;
#elif defined(MV_USE_SERVER_OFI)
typedef struct ofi_server mv_server;
#else
#error("Need server definition (MV_USE_SERVER_IBV | MV_USE_SERVER_OFI)")
#endif

typedef uintptr_t mv_value;
typedef uint64_t mv_key;
typedef void* mv_hash;

struct mv_struct {
  int me;
  int size;
  mv_server* server;
  mv_pool* pkpool;
  mv_hash* tbl;
  mv_am_func_t am_table[128];
  int am_num_func;
} __attribute__((aligned(64)));

#endif
