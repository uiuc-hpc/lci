#ifndef MPIV_MV_H_
#define MPIV_MV_H_

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include "config.h"
#include "macro.h"
#include "ult.h"

int mv_my_worker_id();

/*! Init context */
struct mv_struct;
typedef struct mv_struct mv_engine;
void mv_open(int *argc, char*** args, size_t heap_size, mv_engine**); 
void mv_close(mv_engine*);

/*! Memory function */
void* mv_heap_ptr(mv_engine*);
void mv_set_num_worker(mv_engine*, int number);

/*! Communication function */
typedef void (*mv_am_func_t)();

void mv_send_eager(mv_engine* mv, const void* buffer, int size, int rank, int tag);
void mv_send_rdz(mv_engine* mv, const void* buffer, int size, int rank, int tag, mv_sync* sync);

void mv_recv_eager(mv_engine* mv, void* buffer, int size, int rank, int tag,
                             mv_sync* sync);
void mv_recv_rdz(mv_engine* mv, void* buffer, int size, int rank, int tag,
                             mv_sync* sync);

// void mv_put(mv_engine*, int dst_rank, void* dst, void* src, size_t size);
// void mv_get(mv_engine*, int dst_rank, void* src, void* dst, size_t size, mv_sync* sync);

struct packet;
struct mv_pp;

#if defined(MV_USE_SERVER_RDMAX)
typedef struct rdmax_server mv_server;
#elif defined(MV_USE_SERVER_OFI)
typedef struct ofi_server mv_server;
#else
#error ("Need server definition (MV_USE_SERVER_RDMAX | MV_USE_SERVER_OFI)")
#endif


typedef uintptr_t mv_value;
typedef uint64_t mv_key;
typedef void* mv_hash;

struct mv_struct {
  int me;
  int size;
  mv_server* server;
  mv_hash* tbl;
  mv_pp* pkpool;
  std::vector<mv_am_func_t> am_table;
} __attribute__((aligned(64)));

#endif
