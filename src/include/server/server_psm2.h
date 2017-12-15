#ifndef SERVER_PSM_H_
#define SERVER_PSM_H_

#include <psm2.h>    /* required for core PSM2 functions */
#include <psm2_mq.h> /* required for PSM2 MQ functions (send, recv, etc) */
#include <psm2_am.h>

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "pmi.h"
#include "lc/dequeue.h"
#include "lc/macro.h"

#ifdef WITH_MPI
#include <mpi.h>
#endif

int MPI_Initialized( int *flag );

#define GET_PROTO(p) (p & 0x000000ff)
#define PSM_CTRL_MSG 1

#ifdef LC_SERVER_DEBUG
#define PSM_SAFECALL(x)                                       \
  {                                                           \
    int err = (x);                                            \
    if (err != PSM2_OK && err != PSM2_MQ_NO_COMPLETIONS) {    \
      fprintf(stderr, "err : (%s:%d)\n", __FILE__, __LINE__); \
      exit(err);                                              \
    }                                                         \
  }                                                           \
  while (0)                                                   \
    ;

#else
#define PSM_SAFECALL(x) \
  {                     \
    (x);                \
  }
#endif

#define ALIGNMENT (4096)
#define ALIGNEDX(x) \
  (void*)((((uintptr_t)x + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT))
#define MAX_CQ_SIZE (16 * 1024)
#define MAX_POLL 8

struct psm_server;

enum psm_ctrl_type {
    REQ_TO_REG = 0,
    REQ_TO_PUT,
};

struct psm_ctrl_msg {
  enum psm_ctrl_type type;
  int from;
  uintptr_t addr;
  size_t size;
  uint32_t rkey;
  uint32_t sid;
};

struct psm_mr {
  psm2_mq_req_t req;
  struct psm_server* server;
  uintptr_t addr;
  size_t size;
  uint32_t rkey;
};

typedef struct psm_server {
  psm2_uuid_t uuid;

  // Endpoint + Endpoint ID.
  psm2_ep_t myep;
  psm2_epid_t myepid;

  psm2_epid_t* epid;
  psm2_epaddr_t* epaddr;

  psm2_mq_t mq;
  int psm_recv_am_idx;

  struct psm_ctrl_msg ctrl_msg;
  struct psm_mr reg_mr;
  psm2_mq_req_t put_req;

  uintptr_t* heap_addr;
  void* heap;
  uint32_t heap_rkey;
  size_t recv_posted;
  struct dequeue free_mr;
  lch* mv;
} psm_server __attribute__((aligned(64)));

extern size_t server_max_recvs;

static psm2_mq_tag_t tagsel = {.tag0 = 0x00000000, .tag1 = 0x00000000, .tag2 = 0xFFFFFFFF};

LC_INLINE void psm_init(lch* mv, size_t heap_size, psm_server** s_ptr);
LC_INLINE void psm_post_recv(psm_server* s, lc_packet* p);
LC_INLINE int psm_write_send(psm_server* s, int rank, void* buf, size_t size,
                             lc_packet* ctx, uint32_t proto);

// LC_INLINE void psm_write_rma(psm_server* s, int rank, void* from,
//                              uintptr_t addr, uint32_t rkey, size_t size,
//                             lc_packet* ctx, uint32_t proto);

LC_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, size_t offset, uint32_t rkey, size_t size,
                                    uint32_t sid, lc_packet* ctx);

LC_INLINE void psm_finalize(psm_server* s);
static uint32_t next_key = 2;

#define PSM_RECV_CTRL ((uint64_t) 1 << 60)
#define PSM_RECV ((uint64_t)1 << 61)
#define PSM_SEND ((uint64_t)1 << 62)
#define PSM_RDMA ((uint64_t)1 << 63)

LC_INLINE void prepare_rdma(psm_server* s, struct psm_mr* mr)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = mr->rkey;

  PSM_SAFECALL(psm2_mq_irecv2(
      s->mq, PSM2_MQ_ANY_ADDR, &rtag, /* message tag */
      &tagsel,                        /* message tag mask */
      0,                              /* no flags */
      (void*)mr->addr, mr->size, (void*)(PSM_RDMA | (uintptr_t)mr), &mr->req));
}

LC_INLINE void post_control_msg(psm_server *s) 
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = PSM_CTRL_MSG;

  PSM_SAFECALL(psm2_mq_irecv2(
        s->mq, PSM2_MQ_ANY_ADDR, &rtag,               /* message tag */
        &tagsel, /* message tag mask */
        0,                              /* no flags */
        &s->ctrl_msg, sizeof(struct psm_ctrl_msg), (void*) PSM_RECV_CTRL, &s->reg_mr.req));
}

LC_INLINE uintptr_t _real_psm_reg(psm_server* s, void* buf, size_t size)
{
  struct psm_mr* mr = (struct psm_mr*)malloc(sizeof(struct psm_mr));
  assert(mr && "no more memory");
  mr->server = s;
  mr->addr = (uintptr_t)buf;
  mr->size = size;
  do {
    mr->rkey = (__sync_fetch_and_add(&next_key, 2)) & 0x00ffffff;
    if (mr->rkey == 0) printf("WARNING: wrap over rkey\n");
  } while (mr->rkey < 2);
  // assert(mr->rkey > 2 && "overflow rkey");
  prepare_rdma(s, mr);
  return (uintptr_t)mr;
}

LC_INLINE int _real_psm_free(uintptr_t mem)
{
  struct psm_mr* mr = (struct psm_mr*)mem;
  free(mr);
#if 0
  if (psm2_mq_cancel(&mr->req) == PSM2_OK) {
    psm2_mq_wait2(&mr->req, NULL);
    free(mr);
  } else {
    free(mr);
  }
#endif
  return 1;
}

LC_INLINE uintptr_t psm_rma_reg(psm_server* s, void* buf, size_t size)
{
  return _real_psm_reg(s, buf, size);
}

LC_INLINE int psm_rma_dereg(uintptr_t mem) { return _real_psm_free(mem); }
LC_INLINE uint32_t psm_rma_key(uintptr_t mem)
{
  return ((struct psm_mr*)mem)->rkey;
}

static volatile int psm_start_stop = 0;

static void* psm_startup(void* arg)
{
  psm_server* server = (psm_server*)arg;
  while (!psm_start_stop) psm2_poll(server->myep);
  return 0;
}

LC_INLINE void psm_init(lch* mv, size_t heap_size, psm_server** s_ptr)
{
  // setenv("I_MPI_FABRICS", "ofa", 1);
  // setenv("PSM2_SHAREDCONTEXTS", "0", 1);
  // setenv("PSM2_RCVTHREAD", "0", 1);

  psm_server* s = (psm_server*)malloc(sizeof(struct psm_server));

  int ver_major = PSM2_VERNO_MAJOR;
  int ver_minor = PSM2_VERNO_MINOR;

  PSM_SAFECALL(psm2_init(&ver_major, &ver_minor));

  /* Setup the endpoint options struct */
  struct psm2_ep_open_opts option;
  PSM_SAFECALL(psm2_ep_open_opts_get_defaults(&option));
  option.affinity = 0;  // Disable affinity.

  psm2_uuid_generate(s->uuid);

  /* Attempt to open a PSM2 endpoint. This allocates hardware resources. */
  PSM_SAFECALL(psm2_ep_open(s->uuid, &option, &s->myep, &s->myepid));

  /* Exchange ep addr. */
  // int with_mpi = s->with_mpi = 0;
  // char* lc_mpi = getenv("LC_MPI");
  // if (lc_mpi) with_mpi = s->with_mpi = atoi(lc_mpi);

  char key[256];
  char value[256];
  char name[256];

#ifdef WITH_MPI
  if (with_mpi) {
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    if (MPI_THREAD_MULTIPLE != provided) {
      fprintf(stderr, "Need MPI_THREAD_MULTIPLE\n");
      exit(EXIT_FAILURE);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
    MPI_Comm_size(MPI_COMM_WORLD, &mv->size);
  }
#else
  {
    int spawned;
    PMI_Init(&spawned, &mv->size, &mv->me);
    PMI_KVS_Get_my_name(name, 255);
  }
#endif
  int epid_array_mask[mv->size];

  s->epid = (psm2_epid_t*)calloc(mv->size, sizeof(psm2_epid_t));
  s->epaddr = (psm2_epaddr_t*)calloc(mv->size, sizeof(psm2_epaddr_t));

#ifdef WITH_MPI
  if (with_mpi) {
    for (int i = 0; i < mv->size; i++) {
      psm2_epid_t destaddr;
      MPI_Sendrecv(&s->myepid, sizeof(psm2_epid_t), MPI_BYTE, i, 99, &destaddr,
                   sizeof(psm2_epid_t), MPI_BYTE, i, 99, MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);
      memcpy(&s->epid[i], &destaddr, sizeof(psm2_epid_t));
      epid_array_mask[i] = 1;
    }
    MPI_Barrier(MPI_COMM_WORLD);
  } else
#endif
  {
    sprintf(key, "_LC_KEY_%d", mv->me);
    sprintf(value, "%llu", (unsigned long long)s->myepid);
    PMI_KVS_Put(name, key, value);
    PMI_Barrier();
    for (int i = 0; i < mv->size; i++) {
      sprintf(key, "_LC_KEY_%d", i);
      PMI_KVS_Get(name, key, value, 255);
      psm2_epid_t destaddr;
      sscanf(value, "%llu", (unsigned long long*)&destaddr);
      memcpy(&s->epid[i], &destaddr, sizeof(psm2_epid_t));
      epid_array_mask[i] = 1;
    }
  }

  pthread_t startup_thread;
  pthread_create(&startup_thread, NULL, psm_startup, (void*)s);

  psm2_error_t epid_connect_errors[mv->size];
  PSM_SAFECALL(psm2_ep_connect(s->myep, mv->size, s->epid, epid_array_mask,
                               epid_connect_errors, s->epaddr, 0));

  PMI_Barrier();
  psm_start_stop = 1;
  pthread_join(startup_thread, NULL);

  /* Setup mq for comm */
  PSM_SAFECALL(psm2_mq_init(s->myep, PSM2_MQ_ORDERMASK_NONE, NULL, 0, &s->mq));

  dq_init(&s->free_mr);

  post_control_msg(s);

  s->heap = 0;
  posix_memalign((void**) &s->heap, 4096, heap_size);
  s->recv_posted = 0;
  s->mv = mv;
  *s_ptr = s;
  PMI_Barrier();
  // Do not finalize....
  // PMI_Finalize();
}

LC_INLINE int psm_progress(psm_server* s)
{
  psm2_mq_req_t req;
  psm2_mq_status2_t status;
  psm2_error_t err;
  err = psm2_mq_ipeek2(s->mq, &req, NULL);
  if (err == PSM2_OK) {
    err = psm2_mq_test2(&req, &status);  // we need the status
    uintptr_t ctx = (uintptr_t)status.context;
    if (ctx) {
      if (ctx & PSM_RECV) {
        uint32_t proto = status.msg_tag.tag0; // (status.msg_tag >> 40);
        lc_packet* p = (lc_packet*)(ctx ^ PSM_RECV);
        p->context.from = status.msg_tag.tag1; // (status.msg_tag >> 24) & 0x0000ffff;
        p->context.size = (status.msg_length);
        p->context.tag = proto >> 8;
        lc_serve_recv(s->mv, p, GET_PROTO(proto));
        s->recv_posted--;
      } else if (ctx & PSM_RECV_CTRL) {
        if (s->ctrl_msg.type == REQ_TO_REG) {
          s->reg_mr.addr = s->ctrl_msg.addr;
          s->reg_mr.size = s->ctrl_msg.size;
          s->reg_mr.rkey = s->ctrl_msg.rkey; 
          prepare_rdma(s, &s->reg_mr);
        } else { // REQ_TO_PUT
          psm2_mq_tag_t rtag;
          rtag.tag0 = s->ctrl_msg.sid;
          rtag.tag1 = 0x0;
          rtag.tag2 = s->ctrl_msg.rkey;
          PSM_SAFECALL(psm2_mq_isend2(s->mq, s->epaddr[s->ctrl_msg.from], 0,    /* no flags */
                      &rtag,
                      (void*) (s->ctrl_msg.addr), s->ctrl_msg.size, (void*)(PSM_SEND),
                      &s->put_req));
        }
      } else if (ctx & PSM_SEND) {
        lc_packet* p = (lc_packet*)(ctx ^ PSM_SEND);
        if (p) {
          uint32_t proto = p->context.proto;
          lc_serve_send(s->mv, p, proto);
        } else {
          post_control_msg(s);
        }
      } else if (ctx & PSM_RDMA) { // recv rdma.
        struct psm_mr* mr = (struct psm_mr*)(ctx ^ PSM_RDMA);
        uint32_t imm = status.msg_tag.tag0;
        if (mr == &s->reg_mr) post_control_msg(s);
        if (imm) lc_serve_imm(s->mv, imm);
      } else {
        assert(0 && "Invalid data\n");
      }
    }
  } else {
    sched_yield();
  }

  if (s->recv_posted < server_max_recvs)
    psm_post_recv(s, lc_pool_get_nb(s->mv->pkpool));

#if 0
  // Cleanup stuffs when nothing to do, this improves the reg/dereg a bit.
  struct psm_mr* mr = (struct psm_mr*)dq_pop_bot(&(s->free_mr));
  if (unlikely(mr)) {
    if (psm2_mq_cancel(&mr->req) == PSM2_OK) {
      psm2_mq_wait2(&mr->req, NULL);
      free(mr);
    } else {
      free(mr);
    }
  }
#endif

  return (err == PSM2_OK);
}

LC_INLINE void psm_post_recv(psm_server* s, lc_packet* p)
{
  if (p == NULL)  {
    if (s->recv_posted == server_max_recvs / 2 && !server_deadlock_alert) {
      server_deadlock_alert = 1;
#ifdef LC_SERVER_DEBUG
      printf("WARNING-LC: deadlock alert\n");
#endif
    }
    return;
  }
  psm2_mq_tag_t rtag;
  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = 0x0;

  PSM_SAFECALL(psm2_mq_irecv2(
      s->mq, PSM2_MQ_ANY_ADDR, &rtag,                       /* message tag */
      &tagsel, /* message tag mask */
      0,                              /* no flags */
      &p->data, POST_MSG_SIZE, (void*)(PSM_RECV | (uintptr_t)&p->context),
      (psm2_mq_req_t*)p));

  if (++s->recv_posted == server_max_recvs && server_deadlock_alert)
    server_deadlock_alert = 0;
}

LC_INLINE int psm_write_send(psm_server* s, int rank, void* ubuf, size_t size,
                             lc_packet* ctx, uint32_t proto)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = proto;
  rtag.tag1 = s->mv->me;
  rtag.tag2 = 0x0;

#ifdef LC_SERVER_INLINE
  if (size < 1024) {
    PSM_SAFECALL(psm2_mq_send2(s->mq, s->epaddr[rank], 0, &rtag, ubuf, size));
    lc_serve_send(s->mv, ctx, GET_PROTO(proto));
    return 1;
  } else
#endif
  {
    if (ubuf != ctx->data.buffer) memcpy(ctx->data.buffer, ubuf, size);
    ctx->context.proto = GET_PROTO(proto);
    PSM_SAFECALL(psm2_mq_isend2(s->mq, s->epaddr[rank], 0, /* no flags */
                               &rtag, ctx->data.buffer, size,
                               (void*)(PSM_SEND | (uintptr_t)ctx),
                               (psm2_mq_req_t*)ctx));
    return 0;
  }
}

#if 0
LC_INLINE void psm_write_rma(psm_server* s, int rank, void* from,
                             uintptr_t addr, uint32_t rkey, size_t size,
                             lc_packet* ctx, uint32_t proto)
{
}
#endif

LC_INLINE void psm_get(psm_server* s, int rank, void* buf,
                       uintptr_t addr, size_t offset, uint32_t rkey __UNUSED__, size_t size,
                       uint32_t sid, lc_packet* ctx __UNUSED__) // FIXME
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = sid;
  rtag.tag1 = 0;
  rtag.tag2 = PSM_CTRL_MSG;

  struct psm_mr* mr = (struct psm_mr*)  psm_rma_reg(s, buf, size);
  struct psm_ctrl_msg msg = {REQ_TO_PUT, s->mv->me, addr + offset, size, mr->rkey, sid};

  PSM_SAFECALL(psm2_mq_send2(s->mq, s->epaddr[rank], 0,    /* no flags */
               &rtag, /* tag */
               &msg, sizeof(struct psm_ctrl_msg)));
}

LC_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, size_t offset, uint32_t rkey, size_t size,
                                    uint32_t sid, lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = sid;
  rtag.tag1 = 0;
  rtag.tag2 = rkey;

  struct psm_ctrl_msg mr_req = {REQ_TO_REG, s->mv->me, addr + offset, size, rkey+1, 0};
  rtag.tag2 = PSM_CTRL_MSG;
  PSM_SAFECALL(psm2_mq_send2(s->mq, s->epaddr[rank], 0,    /* no flags */
        &rtag, /* tag */
        &mr_req, sizeof(struct psm_ctrl_msg)));

  rtag.tag2 = rkey + 1;
  PSM_SAFECALL(psm2_mq_isend2(s->mq, s->epaddr[rank], 0,    /* no flags */
        &rtag,
        buf, size, (void*)(PSM_SEND | (uintptr_t)ctx),
        (psm2_mq_req_t*)ctx));
}

LC_INLINE void psm_finalize(psm_server* s)
{
  free(s->heap);
  free(s);
}

LC_INLINE uint32_t psm_heap_rkey(psm_server* s __UNUSED__, int node __UNUSED__)
{
  return 0;
}

LC_INLINE void* psm_heap_ptr(psm_server* s) { return s->heap; }
#define lc_server_init psm_init
#define lc_server_send psm_write_send
#define lc_server_get psm_get
#define lc_server_rma_signal psm_write_rma_signal
#define lc_server_heap_rkey psm_heap_rkey
#define lc_server_heap_ptr psm_heap_ptr
#define lc_server_progress psm_progress
#define lc_server_finalize psm_finalize
#define lc_server_post_recv psm_post_recv

#define lc_server_rma_reg psm_rma_reg
#define lc_server_rma_key psm_rma_key
#define lc_server_rma_dereg psm_rma_dereg
#define _real_server_reg _real_psm_reg
#define _real_server_dereg _real_psm_free

#endif
