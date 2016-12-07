#ifndef SERVER_IBV_H
#define SERVER_IBV_H

#include <mpi.h>

#include "mv.h"
#include "affinity.h"
#include "pool.h"
#include "infiniband/verbs.h"

#include "profiler.h"

#define ALIGNMENT (4096)

#define IBV_SERVER_DEBUG

#ifdef IBV_SERVER_DEBUG
#define IBV_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err) {                                                            \
      printf("err : %d (%s:%d)\n", err, __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                                     \
    }                                                                     \
  } while (0);
#else
#define IBV_SAFECALL(x) {\
  x; \
} while (0);
#endif

struct conn_ctx {
  uint64_t addr;
  uint32_t rkey;
  uint32_t qp_num;
  uint16_t lid;
  union ibv_gid gid;
};

typedef struct ibv_mr mv_server_memory;

typedef struct ibv_server {
  // MV fields.
  mvh* mv;

  // Device fields.
  struct ibv_context* dev_ctx;
  struct ibv_pd* dev_pd;
  struct ibv_srq* dev_srq;
  struct ibv_cq* send_cq;
  struct ibv_cq* recv_cq;
  mv_server_memory* sbuf;
  mv_server_memory* heap;

  // Connections O(N)
  struct ibv_qp** dev_qp;
  struct conn_ctx* conn;

  // Helper fields.
  uint32_t max_inline;
  mv_pool* sbuf_pool;
  void* heap_ptr;
  int recv_posted;
} ibv_server __attribute__((aligned(64)));


void ibv_server_init(mvh* mv, size_t heap_size, ibv_server** s_ptr);
void ibv_server_post_recv(ibv_server* s, mv_packet* p);
void ibv_server_write_send(ibv_server* s, int rank, void* buf, size_t size,
                             void* ctx);
void ibv_server_write_rma(ibv_server* s, int rank, void* from,
                            void* to, uint32_t rkey, size_t size,
                            void* ctx);
void ibv_server_write_rma_signal(ibv_server* s, int rank, void* from,
                                   void* to, uint32_t rkey,
                                   size_t size, uint32_t sid, void* ctx);
void ibv_server_finalize(ibv_server* s);
int ibv_server_progress(ibv_server* s);

mv_server_memory* ibv_server_mem_malloc(ibv_server* s, size_t size);
void ibv_server_mem_free(mv_server_memory* mr);
void* ibv_server_heap_ptr(mv_server* s);
uint32_t ibv_server_heap_rkey(mv_server* s, int node);

#define mv_server_init ibv_server_init
#define mv_server_send ibv_server_write_send
#define mv_server_rma ibv_server_write_rma
#define mv_server_rma_signal ibv_server_write_rma_signal
#define mv_server_heap_rkey ibv_server_heap_rkey
#define mv_server_heap_ptr ibv_server_heap_ptr
#define mv_server_progress ibv_server_progress
#define mv_server_finalize ibv_server_finalize

#endif

