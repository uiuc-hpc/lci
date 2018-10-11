#ifndef SERVER_PSM_H_
#define SERVER_PSM_H_

#include <psm2.h>    /* required for core PSM2 functions */
#include <psm2_mq.h> /* required for PSM2 MQ functions (send, recv, etc) */
#include <psm2_am.h>

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "pm.h"
#include "lc/macro.h"
#include "include/lcrq.h"
#include "config.h"

#define GET_PROTO(p) (p & 0x000000ff)
#define PSM_RDMA_IMM ((uint32_t) 1<<31)

#define PROTO_GET_META(proto)  ((proto >> 16) & 0xffff)

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

#define ALIGNMENT (lcg_page_size)
#define ALIGNEDX(x) \
  (void*)((((uintptr_t)x + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT))
#define MAX_CQ_SIZE (16 * 1024)
#define MAX_POLL 8

#define PSM_RECV_DATA 0x0
#define PSM_RECV_RDMA 0x1
#define PSM_RECV_RDMA_SID 0x2
#define PSM_RECV_RDMA_RTP 0x3

struct psm_server;

struct psm_mr {
  struct psm_server* server;
  uintptr_t addr;
  size_t size;
  uint32_t rkey;
  psm2_mq_req_t req;
};

typedef struct psm_server {
  struct lci_dev* dev;

  psm2_uuid_t uuid;
  int me;
  // struct lci_ep* ep;

  // Endpoint + Endpoint ID.
  psm2_ep_t myep;
  psm2_epid_t myepid;

  // psm2_epid_t* epid;
  // psm2_epaddr_t* epaddr;

  psm2_mq_t mq;

  uintptr_t* heap_addr;
  void* heap;
  uint32_t heap_rkey;
  size_t recv_posted;
  lcrq_t free_mr;
} psm_server __attribute__((aligned(LC_CACHE_LINE)));

static psm2_mq_tag_t LCI_PSM2_TAGSEL = {{0x0, 0x0, 0xFFFFFFFC}};

#define IMM_SERVER_TAG (0xFFFFFFFF)

LC_INLINE void lc_server_post_recv(psm_server* s, lc_packet* p);

#if 0
LC_INLINE void lc_server_rma(psm_server* s, int rank, void* from,
                             uintptr_t addr, uint32_t rkey, size_t size,
                             lc_packet* ctx, uint32_t proto);

LC_INLINE void lc_server_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, size_t offset, uint32_t rkey, size_t size,
                                    uint32_t sid, lc_packet* ctx);

LC_INLINE void lc_server_finalize(psm_server* s);
#endif

static uint32_t next_key = 1;

#define PSM_RECV_CTRL ((uint64_t) 1 << 60)
#define PSM_RECV ((uint64_t)1 << 61)
#define PSM_SEND ((uint64_t)1 << 62)
#define PSM_RDMA ((uint64_t)1 << 63)

LC_INLINE void lc_server_post_recv_rma(psm_server* s, void* addr, size_t size,
    uint32_t rkey, uintptr_t ctx, psm2_mq_req_t* req)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = rkey;

  PSM_SAFECALL(psm2_mq_irecv2(
      s->mq, PSM2_MQ_ANY_ADDR, &rtag, /* message tag */
      &LCI_PSM2_TAGSEL,               /* message tag mask */
      0,                              /* no flags */
      addr, size, (void*)(PSM_RECV | ctx), req));
}

LC_INLINE struct psm_mr* lc_server_get_free_mr(psm_server* s)
{
  struct psm_mr* mr = (struct psm_mr*) lcrq_dequeue(&s->free_mr);
  if (!mr) {
    posix_memalign((void**) &mr, LC_CACHE_LINE, sizeof(struct psm_mr));
  }
  return mr;
}

LC_INLINE uint32_t lc_server_get_free_key()
{
  uint32_t key;
  do {
    key = ((__sync_fetch_and_add(&next_key, 1)) << 2 & 0x00ffffff);
    if (key == 0) printf("WARNING: wrap over rkey\n");
  } while (key < 0b100);
  return key;
}

LC_INLINE uintptr_t _real_psm_reg(psm_server* s, void* buf, size_t size)
{
  struct psm_mr* mr = lc_server_get_free_mr(s);
  mr->server = s;
  mr->addr = (uintptr_t)buf;
  mr->size = size;
  mr->rkey = lc_server_get_free_key();
  lc_server_post_recv_rma(s, buf, size, mr->rkey, 0, &(mr->req));
  return (uintptr_t)mr;
}

LC_INLINE int _real_psm_free(uintptr_t mem)
{
  struct psm_mr* mr = (struct psm_mr*)mem;
  lcrq_enqueue(&mr->server->free_mr, (void*) mr);
  return 1;
}

LC_INLINE uintptr_t lc_server_rma_reg(psm_server* s, void* buf, size_t size)
{
  return _real_psm_reg(s, buf, size);
}

LC_INLINE int lc_server_rma_dereg(uintptr_t mem) { return _real_psm_free(mem); }
LC_INLINE uint32_t lc_server_rma_key(uintptr_t mem)
{
  return ((struct psm_mr*)mem)->rkey;
}

LC_INLINE void lc_server_init(struct lci_dev* dev)
{
  // setenv("I_MPI_FABRICS", "ofa", 1);
  // setenv("PSM2_SHAREDCONTEXTS", "0", 1);
  // setenv("PSM2_RCVTHREAD", "0", 1);
  psm_server* s = NULL;
  posix_memalign((void**) &s, lcg_page_size, sizeof(struct psm_server));

  s->dev = dev;

  int ver_major = PSM2_VERNO_MAJOR;
  int ver_minor = PSM2_VERNO_MINOR;

  static int psm2_initialized = 0;
  if (!psm2_initialized) {
    PSM_SAFECALL(psm2_init(&ver_major, &ver_minor));
    psm2_initialized = 1;
  }

  /* Setup the endpoint options struct */
  struct psm2_ep_open_opts option;
  PSM_SAFECALL(psm2_ep_open_opts_get_defaults(&option));
  option.affinity = 0;  // Disable affinity.

  psm2_uuid_generate(s->uuid);
  dev->name = (char*) calloc(1, 256);

  /* Attempt to open a PSM2 endpoint. This allocates hardware resources. */
  PSM_SAFECALL(psm2_ep_open(s->uuid, &option, &s->myep, &s->myepid));

  dev->handle = (void*) s;
  s->me = lcg_rank;
  s->recv_posted = 0;

  posix_memalign(&s->heap, lcg_page_size,
                 LC_SERVER_NUM_PKTS * LC_PACKET_SIZE * 2 + LC_DEV_MEM_SIZE);

  lcrq_init(&s->free_mr);
  for (int i = 0; i < 256; i++)
    lcrq_enqueue(&s->free_mr, malloc(sizeof(struct psm_mr)));

  PSM_SAFECALL(psm2_mq_init(s->myep, PSM2_MQ_ORDERMASK_ALL, NULL, 0, &s->mq));
}

LC_INLINE void lc_server_ep_publish(psm_server* s, int erank)
{
  char ep_name[256];
  s->heap_rkey = lc_server_get_free_key();
  sprintf(ep_name, "%llu-%llu-%d", (unsigned long long)s->myepid,
          (unsigned long long) s->heap, (uint32_t) s->heap_rkey);
  lc_pm_publish(lcg_rank, erank, ep_name);
}

LC_INLINE void lc_server_connect(psm_server* s, int prank, int erank, lc_rep rep)
{
  char ep_name[256];

  psm2_error_t errs;
  lc_pm_getname(prank, erank, ep_name);
  psm2_epid_t destaddr;
  psm2_epaddr_t epaddr;
  unsigned long long baseaddr;
  uint32_t rkey;
  sscanf(ep_name, "%llu-%llu-%d", (unsigned long long*)&destaddr, (unsigned long long*) &baseaddr,
         &rkey);
  PSM_SAFECALL(psm2_ep_connect(s->myep, 1, &destaddr, NULL,
                               &errs, &epaddr, 0));
  rep->rank = prank;
  rep->gid = erank;
  rep->handle = (void*) epaddr;
  rep->base = (uintptr_t) baseaddr;
  rep->rkey = rkey;
}

LC_INLINE int lc_server_progress(psm_server* s, const long cap)
{
  psm2_mq_req_t req;
  psm2_mq_status2_t status;
  psm2_error_t err = psm2_mq_ipeek2(s->mq, &req, &status);

  if (err == PSM2_OK) {
    #ifndef USE_MINI_PSM2
    psm2_mq_test2(&req, &status);  // we need the status
    #endif

    uintptr_t ctx = (uintptr_t) status.context;
    if (ctx & PSM_RECV) {
      lc_packet* p = (lc_packet*) (ctx ^ PSM_RECV);
      uint32_t pk_type = status.msg_tag.tag2;
      // Simple recv.
      if (pk_type == PSM_RECV_DATA) {
        p->context.req = &p->context.req_s;
        p->context.req->rhandle = status.msg_peer;
        p->context.req->rank = status.msg_tag.tag1;
        p->context.req->size = (status.msg_length);
        uint32_t proto = status.msg_tag.tag0;
        lci_serve_recv(p, proto, cap);
        s->recv_posted--;
      } else if (pk_type == PSM_RECV_RDMA) {
        p->context.req = &p->context.req_s;
        uintptr_t addr = (uintptr_t) status.msg_tag.tag0 + (uintptr_t) s->heap;
        memcpy((void*) addr, p->data.buffer, status.msg_length);
        lc_server_post_recv(s, p);
      } else if (pk_type == PSM_RECV_RDMA_SID) {
        p->context.req = &p->context.req_s;
        uintptr_t addr = (uintptr_t) status.msg_tag.tag0 + (uintptr_t) s->heap;
        memcpy((void*) addr, p->data.buffer, status.msg_length);
        lci_serve_recv_rdma(p, status.msg_tag.tag1);
        s->recv_posted--;
      } else if (pk_type == PSM_RECV_RDMA_RTP) {
        uintptr_t addr = (uintptr_t) status.msg_tag.tag0 + (uintptr_t) s->heap;
        uint32_t rkey = status.msg_tag.tag1;
        size_t size; memcpy(&size, p->data.buffer, sizeof(size_t));
        lc_server_post_recv_rma(s, (void*) addr, size, rkey, (uintptr_t) p, (psm2_mq_req_t*) p);
        s->recv_posted--;
      } else if (status.msg_tag.tag0 & PSM_RDMA_IMM) {
        uint32_t off = status.msg_tag.tag0 ^ PSM_RDMA_IMM;
        lc_packet* p = (lc_packet*) (s->dev->base_addr + off);
        lci_serve_imm(p, cap);
      } else if (status.msg_tag.tag0) {
        p->context.req = &p->context.req_s;
        lci_serve_recv_rdma(p, status.msg_tag.tag1);
      } else {
        p->context.req = &p->context.req_s;
        lc_pool_put(s->dev->pkpool, p);
      }
    } else if (ctx & PSM_SEND) {
      lc_packet* p = (lc_packet*) (ctx ^ PSM_SEND);
      if (p) lci_serve_send(p);
    }
  }

  // Post receive if any.
  if (s->recv_posted < LC_SERVER_MAX_RCVS)
    lc_server_post_recv(s, lc_pool_get_nb(s->dev->pkpool));

  return (err == PSM2_OK);
}

LC_INLINE void lc_server_post_recv(psm_server* s, lc_packet* p)
{
  if (p == NULL)  {
    if (s->recv_posted == LC_SERVER_MAX_RCVS / 2 && !lcg_deadlock) {
      lcg_deadlock = 1;
#ifdef LC_SERVER_DEBUG
      printf("WARNING-LC: deadlock alert\n");
#endif
    }
    return;
  }
  psm2_mq_tag_t rtag;
  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = PSM_RECV_DATA;

  PSM_SAFECALL(psm2_mq_irecv2(
      s->mq, PSM2_MQ_ANY_ADDR, &rtag,                       /* message tag */
      &LCI_PSM2_TAGSEL, /* message tag mask */
      0,       /* no flags */
      &p->data, POST_MSG_SIZE, (void*)(PSM_RECV | (uintptr_t)&p->context),
      (psm2_mq_req_t*)p));

  if (++s->recv_posted == LC_SERVER_MAX_RCVS && lcg_deadlock)
    lcg_deadlock = 0;
}

LC_INLINE int lc_server_sendm(psm_server* s, void* rep, size_t size,
                              lc_packet* ctx, uint32_t proto)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = proto;
  rtag.tag1 = s->me;
  rtag.tag2 = PSM_RECV_DATA;

  PSM_SAFECALL(psm2_mq_isend2(s->mq, rep, 0, /* no flags */
        &rtag, ctx->data.buffer, size,
        (void*)(PSM_SEND | (uintptr_t)ctx),
        (psm2_mq_req_t*)ctx));
  return 0;
}

LC_INLINE int lc_server_sends(psm_server* s, void* rep, void* ubuf, size_t size,
                              uint32_t proto)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = proto;
  rtag.tag1 = s->me;
  rtag.tag2 = PSM_RECV_DATA;
  PSM_SAFECALL(psm2_mq_send2(s->mq, rep, 0, &rtag, ubuf, size));
  return 0;
}

LC_INLINE void lc_server_puts(psm_server* s, void* rep, void* buf,
    uintptr_t base __UNUSED__, uint32_t offset, uint32_t rkey __UNUSED__, size_t size)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = offset;
  rtag.tag1 = 0x0;
  rtag.tag2 = PSM_RECV_RDMA;
  PSM_SAFECALL(psm2_mq_send2(s->mq, rep, 0, &rtag, buf, size));
}

LC_INLINE void lc_server_putss(psm_server* s, void* rep, void* buf,
    uintptr_t base __UNUSED__, uint32_t offset, uint32_t rkey __UNUSED__, uint32_t meta, size_t size)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = offset;
  rtag.tag1 = meta;
  rtag.tag2 = PSM_RECV_RDMA_SID;
  PSM_SAFECALL(psm2_mq_send2(s->mq, rep, 0, &rtag, buf, size));
}

LC_INLINE void lc_server_putm(psm_server* s, void* rep,
    uintptr_t base __UNUSED__, uint32_t offset, uint32_t rkey __UNUSED__, size_t size,
    lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = offset;
  rtag.tag1 = 0x0;
  rtag.tag2 = PSM_RECV_RDMA;
  PSM_SAFECALL(psm2_mq_isend2(s->mq, rep, 0, &rtag, ctx->data.buffer, size,
               (void*) (PSM_SEND | (uintptr_t) ctx), (psm2_mq_req_t*)ctx));
}

LC_INLINE void lc_server_putms(psm_server* s, void* rep,
    uintptr_t base __UNUSED__, uint32_t offset, uint32_t rkey __UNUSED__, size_t size,
    uint32_t meta, lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = offset;
  rtag.tag1 = meta;
  rtag.tag2 = PSM_RECV_RDMA_SID;
  PSM_SAFECALL(psm2_mq_isend2(s->mq, rep, 0, &rtag, ctx->data.buffer, size,
               (void*) (PSM_SEND | (uintptr_t) ctx), (psm2_mq_req_t*)ctx));
}

LC_INLINE void lc_server_putl(psm_server* s, void* rep, void* buffer,
    uintptr_t base __UNUSED__, uint32_t offset, uint32_t rkey, size_t size,
    lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = offset;
  rtag.tag1 = rkey;
  rtag.tag2 = PSM_RECV_RDMA_RTP;
  PSM_SAFECALL(psm2_mq_send2(s->mq, rep, 0, &rtag, &size, sizeof(size_t)));

  rtag.tag0 = 0x0;
  rtag.tag1 = 0x0;
  rtag.tag2 = rkey;
  PSM_SAFECALL(psm2_mq_isend2(s->mq, rep, 0, &rtag, buffer, size,
               (void*) (PSM_SEND | (uintptr_t) ctx), (psm2_mq_req_t*)ctx));
}

LC_INLINE void lc_server_putls(psm_server* s, void* rep, void* buffer,
    uintptr_t base __UNUSED__, uint32_t offset, uint32_t rkey, size_t size,
    uint32_t sid, lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = offset;
  rtag.tag1 = rkey;
  rtag.tag2 = PSM_RECV_RDMA_RTP;
  PSM_SAFECALL(psm2_mq_send2(s->mq, rep, 0, &rtag, &size, sizeof(size_t)));

  rtag.tag0 = 0x1;
  rtag.tag1 = sid;
  rtag.tag2 = rkey;
  PSM_SAFECALL(psm2_mq_isend2(s->mq, rep, 0, &rtag, buffer, size,
               (void*) (PSM_SEND | (uintptr_t) ctx), (psm2_mq_req_t*)ctx));
}
#if 0
LC_INLINE void lc_server_get(psm_server* s, int rank, void* buf,
                       uintptr_t addr, size_t offset, uint32_t rkey __UNUSED__, size_t size,
                       uint32_t sid, lc_packet* ctx __UNUSED__) // FIXME
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = sid;
  rtag.tag1 = 0;
  rtag.tag2 = PSM_CTRL_MSG;

  struct psm_mr* mr = (struct psm_mr*)  lc_server_rma_reg(s, buf, size);
  struct psm_ctrl_msg msg = {REQ_TO_PUT, s->ep->me, addr + offset, size, mr->rkey, sid};

  PSM_SAFECALL(psm2_mq_send2(s->mq, s->epaddr[rank], 0,    /* no flags */
               &rtag, /* tag */
               &msg, sizeof(struct psm_ctrl_msg)));
}
#endif

LC_INLINE void lc_server_rma_rtr(psm_server* s, void* rep, void* buf,
                                 uintptr_t addr __UNUSED__, uint32_t rkey, size_t size,
                                 uint32_t sid, lc_packet* ctx)
{
  psm2_mq_tag_t rtag;
  rtag.tag0 = sid | PSM_RDMA_IMM;
  rtag.tag1 = 0;
  rtag.tag2 = rkey;

  PSM_SAFECALL(psm2_mq_isend2(s->mq, rep, 0,    /* no flags */
        &rtag,
        buf, size, (void*)(PSM_SEND | (uintptr_t)ctx),
        (psm2_mq_req_t*)ctx));
}

#if 0

LC_INLINE void lc_server_finalize(psm_server* s)
{
  free(s->heap);
  free(s);
}

LC_INLINE uint32_t lc_server_heap_rkey(psm_server* s __UNUSED__, int node __UNUSED__)
{
  return 0;
}
#endif

LC_INLINE void* lc_server_heap_ptr(psm_server* s) { return s->heap; }

#define _real_server_reg _real_psm_reg
#define _real_server_dereg _real_psm_free

#endif
