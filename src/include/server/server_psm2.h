#ifndef SERVER_PSM_H_
#define SERVER_PSM_H_

#include <mpi.h>
#include <psm2.h> /* required for core PSM2 functions */
#include <psm2_mq.h> /* required for PSM2 MQ functions (send, recv, etc) */
#include <psm2_am.h>
#include <stdlib.h>
#include <string.h>

#include "mv/macro.h"
#include "dreg/dreg.h"

// #define USE_DREG

// #define SERVER_PSM_DEBUG

#define GET_PROTO(p) (p & 0x00ff)
#define MAKE_PSM_TAG(proto, rank) (((uint64_t) proto << 48) | (uint64_t) rank << 32)

#ifdef SERVER_PSM_DEBUG
#define PSM_SAFECALL(x)                                                   \
  {                                                                       \
    int err = (x);                                                        \
    if (err != PSM2_OK) {                                                  \
      fprintf(stderr, "err : (%s:%d)\n", __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                                     \
    }                                                                     \
  }                                                                       \
  while (0)                                                               \
    ;

#else
#define PSM_SAFECALL(x) \
  {                    \
    (x);               \
  }
#endif

#define ALIGNMENT (4096)
#define ALIGNEDX(x) \
  (void*)((((uintptr_t)x + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT))
#define MAX_CQ_SIZE (16 * 1024)
#define MAX_POLL 8

typedef struct psm_server {
  psm2_uuid_t uuid;

  // Endpoint + Endpoint ID.
  psm2_ep_t myep;
  psm2_epid_t myepid;

  psm2_epid_t* epid;
  psm2_epaddr_t* epaddr;

  psm2_mq_t mq;
  int psm_recv_am_idx;

  uintptr_t* heap_addr;
  void* heap;
  uint32_t heap_rkey;
  int recv_posted;
  mvh* mv;
} psm_server __attribute__((aligned(64)));

struct psm_mr {
  psm2_mq_req_t req;
  uintptr_t addr;
  size_t size;
  uint32_t rkey;
};

MV_INLINE void psm_init(mvh* mv, size_t heap_size, psm_server** s_ptr);
MV_INLINE void psm_post_recv(psm_server* s, mv_packet* p);
MV_INLINE int psm_write_send(psm_server* s, int rank, void* buf, size_t size,
                             mv_packet* ctx, uint32_t proto);
MV_INLINE void psm_write_rma(psm_server* s, int rank, void* from,
                             uintptr_t addr, uint32_t rkey, size_t size,
                             mv_packet* ctx, uint32_t proto);

MV_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, uint32_t rkey, size_t size,
                                    uint32_t sid, mv_packet* ctx, uint32_t proto);

MV_INLINE void psm_finalize(psm_server* s);
MV_INLINE void psm_flush(psm_server* s);

static uint32_t next_key = 1;

#define PSM_RECV ((uint64_t) 1 << 61)
#define PSM_SEND ((uint64_t) 1 << 62)
#define PSM_RDMA ((uint64_t) 1 << 63)

MV_INLINE void prepare_rdma(psm_server* s, struct psm_mr* mr)
{
  PSM_SAFECALL(psm2_mq_irecv(s->mq,
        mr->rkey, /* message tag */
        (uint64_t)(0x00000000ffffffff), /* message tag mask */
        0, /* no flags */
        (void*) mr->addr, mr->size,
        (void*) (PSM_RDMA | (uintptr_t) mr), &mr->req));
}

MV_INLINE uintptr_t _real_psm_reg(psm_server* s, void* buf, size_t size)
{
  struct psm_mr* mr = (struct psm_mr*) malloc(sizeof(struct psm_mr));
  mr->addr = (uintptr_t) buf;
  mr->size = size;
  mr->rkey = next_key++;
  prepare_rdma(s, mr);
  return (uintptr_t)mr;
}

MV_INLINE int _real_psm_free(uintptr_t mem)
{
  struct psm_mr* mr = (struct psm_mr*) mem;
  PSM_SAFECALL(psm2_mq_cancel(&mr->req));
  PSM_SAFECALL(psm2_mq_wait(&mr->req, NULL));
  free(mr);
  return 1;
}

MV_INLINE uintptr_t psm_rma_reg(psm_server* s, void* buf, size_t size)
{
#ifdef USE_DREG
  return (uintptr_t)dreg_register(s, buf, size);
#else
  return _real_psm_reg(s, buf, size);
#endif
}

MV_INLINE int psm_rma_dereg(uintptr_t mem)
{
#ifdef USE_DREG
  dreg_unregister((dreg_entry*)mem);
  return 1;
#else
  return _real_psm_free(mem);
#endif
}

MV_INLINE uint32_t psm_rma_key(uintptr_t mem)
{
#ifdef USE_DREG
  return ((struct psm_mr*)(((dreg_entry*)mem)->memhandle[0]))->rkey;
#else
  return ((struct psm_mr*) mem)->rkey;
#endif
}

static volatile int psm_start_stop = 0;

static void* psm_startup(void* arg)
{
  psm_server* server = (psm_server*) arg;
  while (!psm_start_stop)
    PSM_SAFECALL(psm2_poll(server->myep));
  return 0;
}

static mvh* __mv;
static volatile mv_packet* __p_r = 0;
static volatile int has_data = 0;

static int psm_recv_am(psm2_am_token_t token,
    psm2_amarg_t *args, int nargs,
    void *src, uint32_t len)
{
  if (!__p_r) __p_r = mv_pool_get(__mv->pkpool);
  memcpy((void*) &__p_r->data, src, len);
  has_data = 1;
  return 0;
}

MV_INLINE void psm_init(mvh* mv, size_t heap_size, psm_server** s_ptr)
{
  psm_server* s = (psm_server*) malloc(sizeof(struct psm_server));

  int rc;
  int ver_major = PSM2_VERNO_MAJOR;
  int ver_minor = PSM2_VERNO_MINOR;

#ifdef USE_DREG
  dreg_init();
#endif

  PSM_SAFECALL(psm2_init(&ver_major, &ver_minor));

  /* Setup the endpoint options struct */
  struct psm2_ep_open_opts option;
  PSM_SAFECALL(psm2_ep_open_opts_get_defaults(&option));
  option.affinity = 0; // Disable affinity.

  psm2_uuid_generate(s->uuid);

  /* Attempt to open a PSM2 endpoint. This allocates hardware resources. */
  PSM_SAFECALL(psm2_ep_open(s->uuid, &option, &s->myep, &s->myepid));

  int provided;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
  if (MPI_THREAD_FUNNELED != provided) {
    printf("Need MPI_THREAD_MULTIPLE\n");
    exit(EXIT_FAILURE);
  }

  /* Exchange ep addr. */
  MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
  MPI_Comm_size(MPI_COMM_WORLD, &mv->size);

  int epid_array_mask[mv->size];

  s->epid = (psm2_epid_t*) calloc(mv->size, sizeof(psm2_epid_t));
  s->epaddr = (psm2_epaddr_t*) calloc(mv->size, sizeof(psm2_epaddr_t));

  for (int i = 0; i < mv->size; i++) {
    if (i != mv->me) {
      psm2_epid_t destaddr;
      MPI_Sendrecv(&s->myepid, sizeof(psm2_epid_t), MPI_BYTE, i, 99, &destaddr,
                   sizeof(psm2_epid_t), MPI_BYTE, i, 99, MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);
      memcpy(&s->epid[i], &destaddr, sizeof(psm2_epid_t));
      epid_array_mask[i] = 1;
    } else {
      epid_array_mask[i] = 0;
    }
  }

  pthread_t startup_thread;
  pthread_create(&startup_thread, NULL, psm_startup, (void*) s);

  psm2_error_t epid_connect_errors[mv->size];
  PSM_SAFECALL(psm2_ep_connect(s->myep, mv->size, s->epid,
        epid_array_mask, epid_connect_errors, s->epaddr, 0));

  // Make sure everyone has finished.
  MPI_Barrier(MPI_COMM_WORLD);

  psm_start_stop = 1;
  pthread_join(startup_thread, NULL);

  /* Setup mq for comm */
  PSM_SAFECALL(psm2_mq_init(s->myep, PSM2_MQ_ORDERMASK_NONE, NULL, 0, &s->mq));

  psm2_am_handler_fn_t am[1];
  am[0] = psm_recv_am;

  PSM_SAFECALL(psm2_am_register_handlers(s->myep,
               am,
               1, &s->psm_recv_am_idx));

  // int val;
  // psm2_mq_getopt(s->mq, PSM2_MQ_RNDV_HFI_THRESH, &val);
  // printf("%d\n", val);

  s->heap = 0;
  posix_memalign(&s->heap, 4096, heap_size);

  s->recv_posted = 0;
  __mv = s->mv = mv;
  *s_ptr = s;
}

MV_INLINE void psm_progress(psm_server* s)
{
  psm2_mq_req_t req;
  psm2_mq_status_t status;
  psm2_error_t err;

#ifdef USE_AM
  // NOTE(danghvu): This ugly hack is to ultilize PSM2 AM layer.
  // This saves some memory copy.
  if (!__p_r) __p_r = mv_pool_get_nb(s->mv->pkpool);
  if (__p_r) psm2_poll(s->myep);
  if (has_data) {
    mv_serve_recv(s->mv, (mv_packet*) __p_r);
    __p_r = 0;
    has_data = 0;
  }
#endif

  err = psm2_mq_ipeek(s->mq, &req, NULL);
  if (err == PSM2_OK) {
    err = psm2_mq_test(&req, &status); // we need the status
    uintptr_t ctx = (uintptr_t) status.context;
    if (ctx)
    if (ctx & PSM_RECV) {
      uint32_t proto = (status.msg_tag >> 48);
      mv_packet* p = (mv_packet*) (ctx ^ PSM_RECV);
      p->context.from = (status.msg_tag >> 32) & 0x0000ffff;
      p->context.size = (status.msg_length) - sizeof(struct packet_header);
      p->context.tag = proto >> 8;
      mv_serve_recv(s->mv, p, GET_PROTO(proto));
      s->recv_posted--;
    } else if (ctx & PSM_SEND) {
      mv_packet* p = (mv_packet*) (ctx ^ PSM_SEND);
      uint32_t proto = p->context.proto;
      mv_serve_send(s->mv, p, proto);
    } else { // else if (ctx & PSM_RDMA) { // recv rdma.
      struct psm_mr* mr = (struct psm_mr*) (ctx ^ PSM_RDMA);
      uint32_t imm = (status.msg_tag >> 32);
      prepare_rdma(s, mr);
      if (imm)
        mv_serve_imm(s->mv, imm);
    }
  }

  if (s->recv_posted < MAX_RECV)
    psm_post_recv(s, mv_pool_get_nb(s->mv->pkpool));

}

MV_INLINE void psm_post_recv(psm_server* s, mv_packet* p)
{
  if (p == NULL) return;

  PSM_SAFECALL(psm2_mq_irecv(s->mq,
        0, /* message tag */
        (uint64_t)(0x00000000ffffffff), /* message tag mask */
        0, /* no flags */
        &p->data, POST_MSG_SIZE,
        (void*) (PSM_RECV | (uintptr_t) &p->context),
        (psm2_mq_req_t*) p));
  s->recv_posted++;
}

MV_INLINE int psm_write_send(psm_server* s, int rank, void* ubuf, size_t size,
                             mv_packet* ctx, uint32_t proto)
{
  int me = s->mv->me;

  if (size <= 4096) {
#ifdef USE_AM
    PSM_SAFECALL(psm2_am_request_short(
        s->epaddr[rank], s->psm_recv_am_idx, NULL, 0, buf, size,
        PSM2_AM_FLAG_NOREPLY | PSM2_AM_FLAG_ASYNC, NULL, 0));
#else
    PSM_SAFECALL(psm2_mq_send(s->mq, s->epaddr[rank], 0,
        MAKE_PSM_TAG(proto, me), ubuf, size));
#endif
    mv_serve_send(s->mv, ctx, GET_PROTO(proto));
    return 1;
  } else {
    if (ubuf != ctx->data.buffer)
      memcpy(ctx->data.buffer, ubuf, size);
    ctx->context.proto = GET_PROTO(proto);
    PSM_SAFECALL(psm2_mq_isend(s->mq,
        s->epaddr[rank],
        0, /* no flags */
        MAKE_PSM_TAG(proto, me), ctx->data.buffer, size,
        (void*)(PSM_SEND | (uintptr_t) ctx), (psm2_mq_req_t*) ctx));
    return 0;
  }
}

MV_INLINE void psm_write_rma(psm_server* s, int rank, void* from,
                             uintptr_t addr, uint32_t rkey, size_t size,
                             mv_packet* ctx, uint32_t proto)
{
}

MV_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, uint32_t rkey, size_t size,
                                    uint32_t sid, mv_packet* ctx, uint32_t proto)
{
  ctx->context.proto = proto;
  PSM_SAFECALL(psm2_mq_isend(s->mq,
        s->epaddr[rank],
        0, /* no flags */
        ((uint64_t) sid << 32) | rkey, /* tag */
        buf, size, (void*) (PSM_SEND | (uintptr_t) ctx), (psm2_mq_req_t*) ctx));
}

MV_INLINE void psm_finalize(psm_server* s) { free(s); }

MV_INLINE uint32_t psm_heap_rkey(psm_server* s, int node __UNUSED__)
{
  return 0;
}

MV_INLINE void* psm_heap_ptr(psm_server* s) { return s->heap; }

#define mv_server_init psm_init
#define mv_server_send psm_write_send
#define mv_server_rma psm_write_rma
#define mv_server_rma_signal psm_write_rma_signal
#define mv_server_heap_rkey psm_heap_rkey
#define mv_server_heap_ptr psm_heap_ptr
#define mv_server_progress psm_progress
#define mv_server_finalize psm_finalize
#define mv_server_post_recv psm_post_recv

#define mv_server_rma_reg psm_rma_reg
#define mv_server_rma_key psm_rma_key
#define mv_server_rma_dereg psm_rma_dereg

#endif
