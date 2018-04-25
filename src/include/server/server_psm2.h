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
#include "include/lcrq.h"
#include "config.h"

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
  int me;
  struct lci_ep* ep;

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
  lcrq_t free_mr;
} psm_server __attribute__((aligned(64)));

static psm2_mq_tag_t tagsel = {{0x0, 0x0, 0xFFFFFFFF}};

#define IMM_SERVER_TAG (0xFFFFFFFF)

LC_INLINE void psm_post_recv(psm_server* s, lc_packet* p);
LC_INLINE int psm_write_send(psm_server* s, lc_id rank, void* ubuf, size_t size,
                             lc_packet* ctx, uint32_t proto);

#if 0
LC_INLINE void psm_write_rma(psm_server* s, int rank, void* from,
                             uintptr_t addr, uint32_t rkey, size_t size,
                             lc_packet* ctx, uint32_t proto);

LC_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, size_t offset, uint32_t rkey, size_t size,
                                    uint32_t sid, lc_packet* ctx);

LC_INLINE void psm_finalize(psm_server* s);
#endif

static uint32_t next_key = 2;

#define PSM_RECV_CTRL ((uint64_t) 1 << 60)
#define PSM_RECV ((uint64_t)1 << 61)
#define PSM_SEND ((uint64_t)1 << 62)
#define PSM_RDMA ((uint64_t)1 << 63)

LC_INLINE void psm_post_recv_rma(psm_server* s, void* addr, size_t size, uint32_t rkey)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = rkey;

  PSM_SAFECALL(psm2_mq_irecv2(
      s->mq, PSM2_MQ_ANY_ADDR, &rtag, /* message tag */
      &tagsel,                        /* message tag mask */
      0,                              /* no flags */
      addr, size, (void*)(PSM_RECV), (psm2_mq_req_t*) &s->reg_mr.req));
}

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
  struct psm_mr* mr = (struct psm_mr*) lcrq_dequeue(&s->free_mr);
  if (!mr)
    mr = (struct psm_mr*)malloc(sizeof(struct psm_mr));
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
  lcrq_enqueue(&mr->server->free_mr, (void*) mr);
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

LC_INLINE void psm_init(struct lci_ep* ep, uint32_t* eid)
{
  // setenv("I_MPI_FABRICS", "ofa", 1);
  // setenv("PSM2_SHAREDCONTEXTS", "0", 1);
  // setenv("PSM2_RCVTHREAD", "0", 1);
  int size;
  int rank;

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

  {
    int spawned;
    PMI_Init(&spawned, &size, &rank);
    PMI_KVS_Get_my_name(name, 255);
  }
  int epid_array_mask[size];

  s->epid = (psm2_epid_t*)calloc(size, sizeof(psm2_epid_t));
  s->epaddr = (psm2_epaddr_t*)calloc(size, sizeof(psm2_epaddr_t));

  {
    sprintf(key, "_LC_KEY_%d", rank);
    sprintf(value, "%llu", (unsigned long long)s->myepid);
    PMI_KVS_Put(name, key, value);
    PMI_Barrier();
    for (int i = 0; i < size; i++) {
      sprintf(key, "_LC_KEY_%d", i);
      PMI_KVS_Get(name, key, value, 255);
      psm2_epid_t destaddr;
      sscanf(value, "%llu", (unsigned long long*)&destaddr);
      memcpy(&s->epid[i], &destaddr, sizeof(psm2_epid_t));
      epid_array_mask[i] = 1;
    }
  }

  psm2_error_t epid_connect_errors[size];
  PSM_SAFECALL(psm2_ep_connect(s->myep, size, s->epid, epid_array_mask,
                               epid_connect_errors, s->epaddr, 0));

  PMI_Barrier();

  /* Setup mq for comm */
  PSM_SAFECALL(psm2_mq_init(s->myep, PSM2_MQ_ORDERMASK_ALL, NULL, 0, &s->mq));

  lcrq_init(&s->free_mr);
  for (int i = 0; i < 256; i++)
    lcrq_enqueue(&s->free_mr, malloc(sizeof(struct psm_mr)));

  s->recv_posted = 0;

#if 0
  s->heap = 0;
  posix_memalign((void**) &s->heap, 4096, heap_size);
#endif

  ep->hw.handle = s;
  ep->remotes.n_handle = size;
  ep->remotes.handle = (void**) &s->epaddr[0];
  s->me = *eid = (uint32_t) rank;
  s->ep = ep;

  PMI_Barrier();
  // Do not finalize....
  // PMI_Finalize();
}

LC_INLINE int psm_progress(psm_server* s)
{
  psm2_mq_req_t req;
  psm2_mq_status2_t status;
  psm2_error_t err;
  int count = 0;
  do {
    err = psm2_mq_ipeek2(s->mq, &req, NULL);
  } while (err != PSM2_OK && count++ < 250);
  
  if (err == PSM2_OK) {
    err = psm2_mq_test2(&req, &status);  // we need the status
    uintptr_t ctx = (uintptr_t) status.context;
    if (ctx & PSM_RECV) {
      lc_packet* p = (lc_packet*) (ctx ^ PSM_RECV);

      if (status.msg_tag.tag2 == 0x0) {
        p->context.req = &p->context.req_s;
        p->context.req->rank = status.msg_tag.tag1;
        p->context.req->size = (status.msg_length);
        p->context.req->meta.val = status.msg_tag.tag0 >> 8;
        uint32_t proto = status.msg_tag.tag0 & 0x000000ff;
        lc_serve_recv(s->ep, p, proto);
        s->recv_posted--;
      } else {
        uint32_t imm = status.msg_tag.tag0;
        lc_serve_imm(s->ep, imm);
      }
    } else if (ctx & PSM_SEND) {
      lc_packet* p = (lc_packet*) (ctx ^ PSM_SEND);
      uint32_t imm = status.msg_tag.tag0;
      if (p) {
          uint32_t proto = p->context.proto;
          lc_serve_send(s->ep, p, proto);
      }
    }
  } else {
    sched_yield();
  }

  if (s->recv_posted < SERVER_MAX_RCVS)
    psm_post_recv(s, lc_pool_get_nb(s->ep->pkpool));

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
    if (s->recv_posted == SERVER_MAX_RCVS / 2 && !server_deadlock_alert) {
      server_deadlock_alert = 1;
#ifdef LC_SERVER_DEBUG
      printf("WARNING-LC: deadlock alert\n");
#endif
    }
    return;
  }
#if 1
  psm2_mq_tag_t rtag;
  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = 0x0;

  PSM_SAFECALL(psm2_mq_irecv2(
      s->mq, PSM2_MQ_ANY_ADDR, &rtag,                       /* message tag */
      &tagsel, /* message tag mask */
      0,       /* no flags */
      &p->data, POST_MSG_SIZE, (void*)(PSM_RECV | (uintptr_t)&p->context),
      (psm2_mq_req_t*)p));

  if (++s->recv_posted == SERVER_MAX_RCVS && server_deadlock_alert)
    server_deadlock_alert = 0;
#endif
}

LC_INLINE int psm_write_send(psm_server* s, lc_id rank, void* ubuf, size_t size,
                             lc_packet* ctx, uint32_t proto)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = proto;
  rtag.tag1 = s->me;
  rtag.tag2 = 0x0;

#ifdef LC_SERVER_INLINE
  if (size < 1024) {
    PSM_SAFECALL(psm2_mq_send2(s->mq, s->epaddr[rank], 0, &rtag, ubuf, size));
    lc_serve_send(s->ep, ctx, proto);
    return 1;
  } else
#endif
  {
    if (ubuf != ctx->data.buffer) memcpy(ctx->data.buffer, ubuf, size);
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

#if 0

LC_INLINE void psm_get(psm_server* s, int rank, void* buf,
                       uintptr_t addr, size_t offset, uint32_t rkey __UNUSED__, size_t size,
                       uint32_t sid, lc_packet* ctx __UNUSED__) // FIXME
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = sid;
  rtag.tag1 = 0;
  rtag.tag2 = PSM_CTRL_MSG;

  struct psm_mr* mr = (struct psm_mr*)  psm_rma_reg(s, buf, size);
  struct psm_ctrl_msg msg = {REQ_TO_PUT, s->ep->me, addr + offset, size, mr->rkey, sid};

  PSM_SAFECALL(psm2_mq_send2(s->mq, s->epaddr[rank], 0,    /* no flags */
               &rtag, /* tag */
               &msg, sizeof(struct psm_ctrl_msg)));
}
#endif

LC_INLINE void psm_write_rma_rtr(psm_server* s, int rank, void* buf,
                                 uintptr_t addr __UNUSED__, uint32_t rkey, size_t size,
                                 uint32_t sid, lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = sid;
  rtag.tag1 = 0;
  rtag.tag2 = rkey;

  PSM_SAFECALL(psm2_mq_isend2(s->mq, s->epaddr[rank], 0,    /* no flags */
        &rtag,
        buf, size, (void*)(PSM_SEND | (uintptr_t)ctx),
        (psm2_mq_req_t*)ctx));
}

#if 0
LC_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, size_t offset, uint32_t rkey, size_t size,
                                    uint32_t sid, lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = sid;
  rtag.tag1 = 0;
  rtag.tag2 = rkey;

  struct psm_ctrl_msg mr_req = {REQ_TO_REG, s->ep->me, addr + offset, size, rkey+1, 0};
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
#endif

LC_INLINE void* psm_heap_ptr(psm_server* s) { return s->heap; }
#define lc_server_init psm_init
#define lc_server_send psm_write_send
#define lc_server_get psm_get
#define lc_server_rma_rtr psm_write_rma_rtr
#define lc_server_rma psm_write_rma
#define lc_server_heap_rkey psm_heap_rkey
#define lc_server_heap_ptr psm_heap_ptr
#define lc_server_progress psm_progress
#define lc_server_finalize psm_finalize
#define lc_server_post_recv psm_post_recv

#define lc_server_post_rma psm_post_recv_rma

#define lc_server_rma_reg psm_rma_reg
#define lc_server_rma_key psm_rma_key
#define lc_server_rma_dereg psm_rma_dereg
#define _real_server_reg _real_psm_reg
#define _real_server_dereg _real_psm_free

#endif
