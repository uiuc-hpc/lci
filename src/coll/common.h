#ifndef COL_COMMON_H_
#define COL_COMMON_H_

#include <string.h>
#include <assert.h>

#define LC_COL_SEND 0
#define LC_COL_RECV 1
#define LC_COL_SENDRECV 2
#define LC_COL_OP   3
#define LC_COL_MEM  4
#define LC_COL_FREE 5

static inline int opal_next_poweroftwo(int value)
{
  int power2;

  if (0 == value) {
    return 1;
  }
  power2 = 1 << (8 * sizeof (int) - __builtin_clz(value));

  return power2;
}

static inline void lc_colreq_init(lc_colreq* req)
{
  req->flag = 0;
  req->cur = 0;
  req->total = 0;
  lc_signal((void*) &req->pending[0].sync);
  lc_signal((void*) &req->pending[1].sync);
}

static inline void lc_col_send(
    void* src, size_t size, int rank, int tag, lc_ep ep, lc_colreq* req)
{
  lc_col_sched* op = &(req->next[req->total++]);
  op->src = src;
  op->size = size;
  op->rank = rank;
  op->tag = tag;
  op->ep = ep;
  op->type = LC_COL_SEND;
}

static inline void lc_col_recv(
    void* src, size_t size, int rank, int tag, lc_ep ep, lc_colreq* req)
{
  lc_col_sched* op = &(req->next[req->total++]);
  op->src = src;
  op->size = size;
  op->rank = rank;
  op->tag = tag;
  op->ep = ep;
  op->type = LC_COL_RECV;
}

static inline void lc_col_sendrecv(
    void* src, void* dst, size_t size, int rank, int tag, lc_ep ep, lc_colreq* req)
{
  lc_col_sched* op = &(req->next[req->total++]);
  op->src = src;
  op->dst = dst;
  op->size = size;
  op->rank = rank;
  op->tag = tag;
  op->ep = ep;
  op->type = LC_COL_SENDRECV;
}

static inline void lc_col_op(void *dst, void* src, size_t size, lc_colreq* req)
{
  lc_col_sched* op = &(req->next[req->total++]);
  op->src = src;
  op->dst = dst;
  op->size = size;
  op->type = LC_COL_OP;
}

static inline void lc_col_memmove(void *dst, void* src, size_t size, lc_colreq* req)
{
  lc_col_sched* op = &(req->next[req->total++]);
  op->src = src;
  op->dst = dst;
  op->size = size;
  op->type = LC_COL_MEM;
}

static inline void lc_col_free(void* src, lc_colreq* req)
{
  lc_col_sched* op = &(req->next[req->total++]);
  op->src = src;
  op->type = LC_COL_FREE;
}

void lc_col_progress(lc_colreq* req)
{
  // Do nothing until current pending op is done.
  if (!req->pending[0].sync || !req->pending[1].sync) return;
  while (req->cur < req->total) {
    lc_col_sched* op = &(req->next[req->cur++]);
    switch (op->type) {
      case LC_COL_SEND:
        lc_reset((void*) &req->pending[0].sync);
        LC_SAFE(lc_send(op->src, op->size, op->rank, op->tag, op->ep, lc_signal, (void*) &(req->pending[0].sync)));
        break;
      case LC_COL_RECV:
        lc_recv(op->src, op->size, op->rank, op->tag, op->ep, &req->pending[0]);
        break;
      case LC_COL_SENDRECV:
        lc_reset((void*) &req->pending[0].sync);
        LC_SAFE(lc_send(op->src, op->size, op->rank, op->tag, op->ep, lc_signal, (void*) &req->pending[0].sync));
        lc_recv(op->dst, op->size, op->rank, op->tag, op->ep, &req->pending[1]);
        break;
      case LC_COL_OP:
        req->op(op->dst, op->src, op->size);
        break;
      case LC_COL_MEM:
        memmove(op->dst, op->src, op->size);
        break;
      case LC_COL_FREE:
        free(op->src);
        break;
      default:
        assert(0 && "Invalid op");
    }
    // There is now pending ops, return;
    if (!req->pending[0].sync || !req->pending[1].sync) return;
  }
  req->flag = 1;
}

#endif
