// Copyright (c) 2013, Adam Morrison and Yehuda Afek.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//  * Neither the name of the Tel Aviv University nor the names of the
//    author of this software may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>

// Definition: RING_STATS
// --------------------
// Define to collect statistics about CRQ closes and nodes
// marked unsafe.
//#define RING_STATS

// Definition: HAVE_HPTRS
// --------------------
// Define to enable hazard pointer setting for safe memory
// reclamation.  You'll need to integrate this with your
// hazard pointers implementation.
//#define HAVE_HPTRS

#define FAA64(a, b) __sync_fetch_and_add(a, b)
#define CAS64(a, b, c) __sync_bool_compare_and_swap(a, b, c)

/** Indicate a branch is likely to be taken. */
#define likely(x) __builtin_expect(!!(x), 1)
/** Indicate a branch is unlikely to be taken. */
#define unlikely(x) __builtin_expect(!!(x), 0)
/** Indicate address A should be prefetched for a read. */
#define read_prefetch(A) __builtin_prefetch((const void*)A, 0, 3)
/** Indicate address A should be prefetched for a write. */
#define write_prefetch(A) \
{                       \
}  //__builtin_prefetch((const void*) A, 1, 3);

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define __CAS2(ptr, o1, o2, n1, n2)                             \
  ({                                                              \
   char __ret;                                                 \
   __typeof__(o2) __junk;                                      \
   __typeof__(*(ptr)) __old1 = (o1);                           \
   __typeof__(o2) __old2 = (o2);                               \
   __typeof__(*(ptr)) __new1 = (n1);                           \
   __typeof__(o2) __new2 = (n2);                               \
   asm volatile("lock cmpxchg16b %2;setz %1"                   \
       : "=d"(__junk), "=a"(__ret), "+m" (*ptr)     \
       : "b"(__new1), "c"(__new2),                  \
       "a"(__old1), "d"(__old2));                 \
   __ret; })

#ifdef DEBUG
#define CAS2(ptr, o1, o2, n1, n2)                               \
  ({                                                              \
   int res;                                                    \
   res = __CAS2(ptr, o1, o2, n1, n2);                          \
   __executed_cas[__stats_thread_id].v++;                      \
   __failed_cas[__stats_thread_id].v += 1 - res;               \
   res;                                                        \
   })
#else
#define CAS2(ptr, o1, o2, n1, n2)    __CAS2(ptr, o1, o2, n1, n2)
#endif


#define BIT_TEST_AND_SET(ptr, b)                                \
  ({                                                              \
   char __ret;                                                 \
   asm volatile("lock btsq $63, %0; setnc %1" : "+m"(*ptr), "=a"(__ret) : : "cc"); \
   __ret;                                                      \
   })

static inline int is_empty(uint64_t v) __attribute__ ((pure));
static inline uint64_t node_index(uint64_t i) __attribute__ ((pure));
static inline uint64_t set_unsafe(uint64_t i) __attribute__ ((pure));
static inline uint64_t node_unsafe(uint64_t i) __attribute__ ((pure));
static inline uint64_t tail_index(uint64_t t) __attribute__ ((pure));
static inline int crq_is_closed(uint64_t t) __attribute__ ((pure));

#include "lcrq.h"

#define EMPTY NULL;

static inline void init_ring(RingQueue *r) {
  unsigned i;
  for (i = 0; i < RING_SIZE; i++) {
    r->array[i].val = -1;
    r->array[i].idx = i;
  }

  r->head = r->tail = 0;
  r->next = NULL;
}

static inline int is_empty(uint64_t v)  {
  return (v == (uint64_t)-1);
}


static inline uint64_t node_index(uint64_t i) {
  return (i & ~(1ull << 63));
}


static inline uint64_t set_unsafe(uint64_t i) {
  return (i | (1ull << 63));
}


static inline uint64_t node_unsafe(uint64_t i) {
  return (i & (1ull << 63));
}


static inline uint64_t tail_index(uint64_t t) {
  return (t & ~(1ull << 63));
}


static inline int crq_is_closed(uint64_t t) {
  return (t & (1ull << 63)) != 0;
}


static inline void fixState(RingQueue *rq) {

  uint64_t t, h;

  while (1) {
    t = FAA64(&rq->tail, 0);
    h = FAA64(&rq->head, 0);

    if (unlikely((uint64_t) rq->tail != t))
      continue;

    if (h > t) {
      if (CAS64(&rq->tail, t, h)) break;
      continue;
    }
    break;
  }
}

__thread RingQueue *nrq;
__thread RingQueue *hazardptr;

static inline int close_crq(RingQueue *rq, const uint64_t t, const int tries) {
  if (tries < 10)
    return CAS64(&rq->tail, t + 1, (t + 1)|(1ull<<63));
  else
    return BIT_TEST_AND_SET(&rq->tail, 63);
}

void lcrq_destroy(lcrq_t* q)
{
  free(q->head);
}

void lcrq_init(lcrq_t* q) {
  RingQueue *rq = (RingQueue*) malloc(sizeof(RingQueue));
  init_ring(rq);
  q->head = q->tail = rq;
}

void lcrq_enqueue(lcrq_t* q, void* arg) {

  int try_close = 0;
  RingQueue* tail = q->tail;

  while (1) {
    RingQueue *rq = tail;

#ifdef HAVE_HPTRS
    SWAP(&hazardptr, rq);
    if (unlikely(tail != rq))
      continue;
#endif

    RingQueue *next = rq->next;

    if (unlikely(next != NULL)) {
      CAS64(&tail, rq, next);
      continue;
    }

    uint64_t t = FAA64(&rq->tail, 1);

    if (crq_is_closed(t)) {
alloc:
      if (nrq == NULL) {
        nrq = (RingQueue*) malloc(sizeof(RingQueue));
        init_ring(nrq);
      }

      // Solo enqueue
      nrq->tail = 1;
      nrq->array[0].val = (int64_t) arg;
      nrq->array[0].idx = 0;

      if (CAS64(&rq->next, NULL, nrq)) {
        CAS64(&tail, rq, nrq);
        nrq = NULL;
        return;
      }
      continue;
    }

    RingNode* cell = &rq->array[t & (RING_SIZE-1)];
    write_prefetch(cell);

    uint64_t idx = cell->idx;
    uint64_t val = cell->val;

    if (likely(is_empty(val))) {
      if (likely(node_index(idx) <= t)) {
        if ((likely(!node_unsafe(idx)) || (uint64_t) rq->head < t) && CAS2((uint64_t*)cell, -1, idx, (int64_t) arg, t)) {
          return;
        }
      }
    }

    uint64_t h = rq->head;

    if (unlikely((int64_t)(t - h) >= (int64_t)RING_SIZE) && close_crq(rq, t, ++try_close)) {
      goto alloc;
    }
  }
}

void* lcrq_dequeue(lcrq_t* q) {
  // quickly return empty.
  if ((tail_index(q->head->tail) < q->head->head + 1) && q->head->next == NULL)
    return EMPTY;

  while (1) {
    RingQueue *rq = q->head;
    RingQueue *next;

#ifdef HAVE_HPTRS
    SWAP(&hazardptr, rq);
    if (unlikely(head != rq))
      continue;
#endif

    uint64_t h = FAA64(&rq->head, 1);
    RingNode* cell = &rq->array[h & (RING_SIZE-1)];
    write_prefetch(cell);

    uint64_t tt = 0;
    int r = 0;

    while (1) {

      uint64_t cell_idx = cell->idx;
      uint64_t unsafe = node_unsafe(cell_idx);
      uint64_t idx = node_index(cell_idx);
      uint64_t val = cell->val;

      if (unlikely(idx > h)) break;

      if (likely(!is_empty(val))) {
        if (likely(idx == h)) {
          if (CAS2((uint64_t*)cell, val, cell_idx, -1, unsafe | (h + RING_SIZE)))
            return (void*) val;
        } else {
          if (CAS2((uint64_t*)cell, val, cell_idx, val, set_unsafe(idx))) {
            break;
          }
        }
      } else {
        if ((r & ((1ull << 10) - 1)) == 0)
          tt = rq->tail;

        // Optimization: try to bail quickly if queue is closed.
        int crq_closed = crq_is_closed(tt);
        uint64_t t = tail_index(tt);

        if (unlikely(unsafe)) { // Nothing to do, move along
          if (CAS2((uint64_t*)cell, val, cell_idx, val, unsafe | (h + RING_SIZE)))
            break;
        } else if (t < h + 1 || r > 200000 || crq_closed) {
          if (CAS2((uint64_t*)cell, val, idx, val, h + RING_SIZE)) {
            if (r > 200000 && tt > RING_SIZE)
              BIT_TEST_AND_SET(&rq->tail, 63);
            break;
          }
        } else {
          ++r;
        }
      }
    }

    if (tail_index(rq->tail) <= h + 1) {
      fixState(rq);
      // try to return empty
      next = rq->next;
      if (next == NULL)
        return EMPTY;  // EMPTY

      if (tail_index(rq->tail) <= h + 1) {
        if (CAS64(&q->head, rq, next)) {
          // FIXME(danghvu); LEAK rq here, no reclaimation at the moment.
        }
      }
    }
  }
}

#if 0
int main(int argc, char** args) {
  mpmc_queue_t q;
  queue_init(&q);
  enqueue(&q, (void*) 1234);
  printf("%d\n", dequeue(&q));
  return 0;
}
#endif
