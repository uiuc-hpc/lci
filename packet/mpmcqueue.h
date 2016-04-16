/**
 * @file mpmcqueue.h
 * @brief Unbounded MPMC queue.
 */

// This is based on the implementation at:
// http://mcg.cs.tau.ac.il/projects/lcrq/
// See also the paper:
// A. Morrison and Y. Afek. "Fast concurrent queues for x86 processors." PPoPP
// 2013.
// The implementation has the following copyright and license:
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

#ifndef MPMCQUEUE_H_
#define MPMCQUEUE_H_

#include <stddef.h>
#include <vector>
#include <forward_list>

#define CAS2(ptr, o1, o2, n1, n2)                                       \
  ({                                                                    \
    char __ret;                                                         \
    __typeof__(o2) __junk;                                              \
    __typeof__(*(ptr)) __old1 = (o1);                                   \
    __typeof__(o2) __old2 = (o2);                                       \
    __typeof__(*(ptr)) __new1 = (n1);                                   \
    __typeof__(o2) __new2 = (n2);                                       \
    asm volatile("lock cmpxchg16b %2;setz %1"                           \
                 : "=d"(__junk), "=a"(__ret), "+m"(*ptr)                \
                 : "b"(__new1), "c"(__new2), "a"(__old1), "d"(__old2)); \
    __ret;                                                              \
  })

#define BIT_TEST_AND_SET(ptr, b)               \
  ({                                           \
    char __ret;                                \
    asm volatile("lock btsq $63, %0; setnc %1" \
                 : "+m"(*ptr), "=a"(__ret)     \
                 :                             \
                 : "cc");                      \
    __ret;                                     \
  })

#define FETCH_ADD(a, b) __sync_fetch_and_add(a, b)

#define SWAP(a, b) __sync_lock_test_and_set((long*)a, (long)b)

#define CAS(a, b, c) __sync_bool_compare_and_swap(a, b, c)

#define RING_SIZE (1ull << 16)
static_assert(RING_SIZE > (MAX_SEND + MAX_RECV),
              "Queue is bounded by max concurrency");

#define CLOSE_TRIES 10

namespace ppl {

/**
 * General multiple-producer, multiple-consumer queue.
 * @warning Value *must* be 64 bits.
 */
template <typename Value>
class MPMCQueue final {
 public:
  /**
   * Create an empty queue.
   */
  MPMCQueue();

  /**
   * Delete this queue.
   */
  ~MPMCQueue();

  void enqueue(Value n);
  Value dequeue();
  void enqueue_nosync(Value n) { enqueue(n); }
  Value dequeue_nosync() { return dequeue(); }
  // WARNING: This may report the queue is not empty when it is.
  bool empty() const;
  void reclaim();
  static inline Value get_default() { return (Value)0; }
  __attribute__((const));

 private:
  /** Node in a ring queue. */
  struct RingNode {
    volatile Value val;
    volatile uint64_t idx;
  } __attribute__((aligned(128)));
  /** A ring queue. */
  struct RingQueue {
    volatile uint64_t head __attribute__((aligned(64)));
    volatile uint64_t tail __attribute__((aligned(64)));
    RingQueue* next __attribute__((aligned(128)));
    RingNode array[RING_SIZE];
  } __attribute__((aligned(128)));

  /** Head of the linked list of CRQs. */
  RingQueue* head;
  /** Tail of the linked list of CRQs. */
  RingQueue* tail;
  /** Per-thread retired RingQueues. */
  std::vector<std::forward_list<RingQueue*> > retired;

  /** Initialize a new ring queue. */
  inline void init_ring(RingQueue* rq);

  // Helper methods for working on ring nodes.
  inline bool is_empty(Value v) const __attribute__((const));
  inline uint64_t node_index(uint64_t i) const __attribute__((const));
  inline uint64_t set_unsafe(uint64_t i) const __attribute__((const));
  inline uint64_t node_unsafe(uint64_t i) const __attribute__((const));
  inline uint64_t tail_index(uint64_t i) const __attribute__((const));
  inline bool is_crq_closed(uint64_t t) const __attribute__((const));
  inline bool cas2_put_node(RingNode* node, uint64_t old_idx, Value val,
                            uint64_t idx);
  inline bool cas2_take_node(RingNode* node, uint64_t val, uint64_t old_idx,
                             uint64_t new_idx);
  inline bool cas2_idx(RingNode* node, uint64_t val, uint64_t old_idx,
                       uint64_t new_idx);

  /**
   * Fix a RingQueue in which a dequeuer has has made head > tail.
   * @param rq The RingQueue to fix.
   */
  inline void fix_state(RingQueue* rq);
  /**
   * Attempt to close a ring queue.
   * @param rq The RingQueue to close.
   * @param tail The old tail value.
   * @param tries Current number of close attempts.
   */
  inline bool close_crq(RingQueue* rq, const uint64_t tail, const int tries);
};

}  // namespace ppl

#include "mpmcqueue-inl.h"

#endif  // MPMCQUEUE_H_
