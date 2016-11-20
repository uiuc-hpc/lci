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
struct MPIV_Request; 
void mv_send(mv_engine*, const void* buffer, size_t size, int rank, int tag);
void mv_recv(mv_engine*, void* buffer, size_t size, int rank, int tag);
void mv_isend(mv_engine*, const void* buffer, size_t size, int rank, int tag, MPIV_Request*);
void mv_irecv(mv_engine*, void* buffer, size_t size, int rank, int tag, MPIV_Request*);
void mv_waitall(mv_engine*, int count, MPIV_Request* req);

struct packet;
struct mv_pp;

#if defined(MV_USE_SERVER_RDMAX)
typedef struct rdmax_server mv_server;
#elif defined(MV_USE_SERVER_OFI)
typedef struct ofi_server mv_server;
#else
#error ("Need server definition (MV_USE_SERVER_RDMAX | MV_USE_SERVER_OFI)")
#endif

typedef void (*mv_am_func_t)();

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
