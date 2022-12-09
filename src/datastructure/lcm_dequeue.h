#ifndef LCM_DEQUEUE_H
#define LCM_DEQUEUE_H

#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#define LCM_SUCCESS 1
#define LCM_RETRY 0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LCM_dequeue_t {
  size_t top;
  size_t bot;
  size_t length;
  void** container;  // a pointer to type void*
} LCM_dequeue_t;

static inline void LCM_dq_init(LCM_dequeue_t* dq, size_t capacity);
static inline void LCM_dq_finalize(LCM_dequeue_t* dq);
static inline size_t LCM_dq_size(LCM_dequeue_t dq);
static inline size_t LCM_dq_capacity(LCM_dequeue_t dq);
static inline int LCM_dq_push_top(LCM_dequeue_t* dq, void* p);
static inline int LCM_dq_push_bot(LCM_dequeue_t* dq, void* p);
static inline void* LCM_dq_pop_top(LCM_dequeue_t* dq);
static inline void* LCM_dq_pop_bot(LCM_dequeue_t* dq);
static inline void* LCM_dq_peek_bot(LCM_dequeue_t* dq);

#ifdef __cplusplus
}
#endif

static inline void LCM_dq_init(LCM_dequeue_t* dq, size_t capacity)
{
  int ret = posix_memalign((void**)&(dq->container), 64,
                           (capacity + 1) * sizeof(void*));
  assert(ret == 0);
  dq->top = 0;
  dq->bot = 0;
  dq->length = capacity + 1;
}

static inline void LCM_dq_finalize(LCM_dequeue_t* dq)
{
  free(dq->container);
  dq->container = NULL;
}

static inline size_t LCM_dq_size(LCM_dequeue_t dq)
{
  return (dq.top + dq.length - dq.bot) % dq.length;
}

static inline size_t LCM_dq_capacity(LCM_dequeue_t dq) { return dq.length - 1; }

static inline int LCM_dq_push_top(LCM_dequeue_t* dq, void* p)
{
  size_t new_top = (dq->top + 1) % dq->length;
  if (new_top == dq->bot) {
    return LCM_RETRY;
  }
  dq->container[dq->top] = p;
  dq->top = new_top;
  return LCM_SUCCESS;
}

static inline int LCM_dq_push_bot(LCM_dequeue_t* dq, void* p)
{
  size_t new_bot = (dq->bot + dq->length - 1) % dq->length;
  if (dq->top == new_bot) {
    return LCM_RETRY;
  }
  dq->bot = new_bot;
  dq->container[dq->bot] = p;
  return LCM_SUCCESS;
}

static inline void* LCM_dq_pop_top(LCM_dequeue_t* dq)
{
  void* ret = NULL;
  if (dq->top != dq->bot) {
    dq->top = (dq->top + dq->length - 1) % dq->length;
    ret = dq->container[dq->top];
  }
  return ret;
}

static inline void* LCM_dq_pop_bot(LCM_dequeue_t* dq)
{
  void* ret = NULL;
  if (dq->top != dq->bot) {
    ret = dq->container[dq->bot];
    dq->bot = (dq->bot + 1) % dq->length;
  }
  return ret;
}

static inline void* LCM_dq_peek_bot(LCM_dequeue_t* dq)
{
  void* ret = NULL;
  if (dq->top != dq->bot) {
    ret = dq->container[dq->bot];
  }
  return ret;
}

static inline size_t LCM_dq_steal(LCM_dequeue_t* dst, LCM_dequeue_t* src)
{
  size_t src_size = LCM_dq_size(*src);
  size_t dst_size = LCM_dq_size(*dst);
  if (src_size <= dst_size) return 0;
  size_t size_to_steal = (src_size - dst_size) / 2;
  for (int i = 0; i < size_to_steal; ++i) {
    dst->container[dst->top] = src->container[src->bot];
    src->bot = (src->bot + 1) % src->length;
    dst->top = (dst->top + 1) % dst->length;
  }
  return size_to_steal;
}

#endif
