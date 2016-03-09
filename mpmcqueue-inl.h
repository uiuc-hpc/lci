/**
 * @file mpmcqueue-inl.h
 * @brief MPMC queue implementation.
 */

#include <stddef.h>
#include <stdexcept>

/** Indicate a branch is likely to be taken. */
#define likely(x) __builtin_expect(!!(x), 1)
/** Indicate a branch is unlikely to be taken. */
#define unlikely(x) __builtin_expect(!!(x), 0)
/** Indicate address A should be prefetched for a read. */
#define read_prefetch(A) __builtin_prefetch((const void*) A, 0, 3)
/** Indicate address A should be prefetched for a write. */
#define write_prefetch(A) {} //__builtin_prefetch((const void*) A, 1, 3);

namespace ppli {
  constexpr size_t num_workers = 1;
}

namespace ppl {

constexpr int get_core() {return 0;}

template <typename Value>
inline MPMCQueue<Value>::MPMCQueue() { 
  head = new RingQueue();
  init_ring(head);
  tail = head;
}

template <typename Value>
inline MPMCQueue<Value>::~MPMCQueue() {
  reclaim();
  for (RingQueue* rq = head; rq != NULL;) {
    RingQueue* next = rq->next;
    delete rq;
    rq = next;
  }
}

template <typename Value>
inline void MPMCQueue<Value>::enqueue(Value n) {
  // int close_tries = 0;  // Current number of attempts to close CRQ.

  while (true) {
    RingQueue* rq = tail;  // HAZARD.
    RingQueue* next = rq->next;
    if (unlikely(next != NULL)) {
      // Tail has shifted, help move it along.
      CAS(&tail, rq, next);
      continue;
    }

    // Attempt to enqueue in CRQ rq.
    uint64_t crq_tail = FETCH_ADD(&rq->tail, 1);
    /*if (unlikely(is_crq_closed(crq_tail))) {
      throw std::runtime_error("Not enough space, pool is bounded.");
    }*/

    RingNode* node = &rq->array[crq_tail & (RING_SIZE-1)];
    write_prefetch(node);

    // Attempt enqueue.
    uint64_t idx = node->idx;
    Value val = node->val;
    if (likely(is_empty(val))) {
      if (likely(node_index(idx) <= crq_tail)) {
        if ((likely(!node_unsafe(idx)) || rq->head < crq_tail) &&
            cas2_put_node(node, idx, n, crq_tail)) {
          // If we got here, we failed to append a new CRQ if we tried.
          return;
        }
      }
    }

    // Enqueue failed. Possibly close CRQ.
    // uint64_t crq_head = rq->head;
    //if (unlikely((int64_t) (crq_tail - crq_head) >= (int64_t) RING_SIZE) &&
    //    close_crq(rq, crq_tail, ++close_tries)) {
    throw std::runtime_error("Not enough space, pool is bounded.");
    //}
  }
}

template <typename Value>
inline Value MPMCQueue<Value>::dequeue() {
  while (true) {
    RingQueue* rq = head;  // HAZARD.
    uint64_t crq_head = FETCH_ADD(&rq->head, 1);
    RingNode* node = &rq->array[crq_head & (RING_SIZE-1)];
    write_prefetch(node);
    int r = 0;
    uint64_t crq_tail = (uint64_t) -1;

    // Attempt to dequeue from CRQ rq.
    while (true) {
      uint64_t raw_idx = node->idx;
      uint64_t unsafe = node_unsafe(raw_idx);
      uint64_t idx = node_index(raw_idx);
      Value val = node->val;

      // Dequeue fails, might be empty.
      if (unlikely(idx > crq_head)) break;

      if (likely(!is_empty(val))) {
        if (likely(idx == crq_head)) {
          if (cas2_take_node(node, val, raw_idx, unsafe | (crq_head + RING_SIZE))) {
            // Dequeue succeeded.
            return val;
          }
        } else {
          if (cas2_idx(node, val, raw_idx, set_unsafe(idx))) {
            // Node marked unsafe.
            break;
          }
        }
      } else {
        if ((r & ((1ull << 10) - 1)) == 0) {
          crq_tail = rq->tail;
        }

        // Attempt to bail quickly if the CRQ is closed.
        uint64_t tail_idx = tail_index(crq_tail);

        if (unlikely(unsafe)) {
          // Nothing to do here.
          if (cas2_idx(node, val, raw_idx, unsafe | (crq_head + RING_SIZE))) {
            break;
          }
        } else if (tail_idx < crq_head + 1 || r > 200000 ) {
          if (cas2_idx(node, val, idx, crq_head + RING_SIZE)) {
            if (r > 200000 && crq_tail > RING_SIZE) {
              BIT_TEST_AND_SET(&rq->tail, 63);
            }
            break;
          }
        } else {
          ++r;
        }
      }
    }

    if (tail_index(rq->tail) <= crq_head + 1) {
      fix_state(rq);
      RingQueue* next = rq->next;
      if (next == NULL) {
        // Queue is empty.
        return MPMCQueue<Value>::get_default();
      }
      if (tail_index(rq->tail) <= crq_head + 1) {
        throw std::runtime_error("Impossiboru!!! Pool is bounded");
      }
    }
  }
}

template <typename Value>
inline bool MPMCQueue<Value>::empty() const {
  return (tail_index(head->tail) < head->head + 1) && head->next == NULL;
}

template <typename Value>
void MPMCQueue<Value>::reclaim() {
  // nothing to reclaim.
}

template <typename Value>
inline void MPMCQueue<Value>::init_ring(RingQueue* rq) {
  for (size_t i = 0; i < RING_SIZE; ++i) {
    rq->array[i].val = MPMCQueue<Value>::get_default();
    rq->array[i].idx = i;
  }
  rq->head = 0;
  rq->tail = 0;
  rq->next = NULL;
}

template <typename Value>
inline bool MPMCQueue<Value>::is_empty(Value v) const {
  return v == MPMCQueue<Value>::get_default();
}

template <typename Value>
inline uint64_t MPMCQueue<Value>::node_index(uint64_t i) const {
  return i & ~(1ull << 63);
}

template <typename Value>
inline uint64_t MPMCQueue<Value>::set_unsafe(uint64_t i) const {
  return i | (1ull << 63);
}

template <typename Value>
inline uint64_t MPMCQueue<Value>::node_unsafe(uint64_t i) const {
  return i & (1ull << 63);
}

template <typename Value>
inline uint64_t MPMCQueue<Value>::tail_index(uint64_t i) const {
  return i & ~(1ull << 63);
}

template <typename Value>
inline bool MPMCQueue<Value>::is_crq_closed(uint64_t t) const {
  return (t & (1ull << 63)) != 0;
}

template <typename Value>
inline bool MPMCQueue<Value>::cas2_put_node(RingNode* node, uint64_t old_idx,
                                            Value val, uint64_t idx) {
  return CAS2((uint64_t*) node, MPMCQueue<Value>::get_default(), old_idx, val, idx);
}

template <typename Value>
inline bool MPMCQueue<Value>::cas2_take_node(RingNode* node, uint64_t val,
                                             uint64_t old_idx, uint64_t new_idx) {
  return CAS2((uint64_t*) node, val, old_idx, MPMCQueue<Value>::get_default(), new_idx);
}

template <typename Value>
inline bool MPMCQueue<Value>::cas2_idx(RingNode* node, uint64_t val,
                                               uint64_t old_idx, uint64_t new_idx) {
  return CAS2((uint64_t*) node, val, old_idx, val, new_idx);
}

template <typename Value>
inline void MPMCQueue<Value>::fix_state(RingQueue* rq) {
  while (true) {
    uint64_t tail = FETCH_ADD(&rq->tail, 0);
    uint64_t head = FETCH_ADD(&rq->head, 0);
    if (unlikely(rq->tail != tail)) continue;
    if (head > tail) {
      if (CAS(&rq->tail, tail, head)) break;
      continue;
    }
    break;
  }
}

template <typename Value>
inline bool MPMCQueue<Value>::close_crq(RingQueue* rq,
                                        const uint64_t tail,
                                        const int tries) {
  if (tries < CLOSE_TRIES) {
    return CAS(&rq->tail, tail + 1, (tail + 1) | (1ull << 63));
  } else {
    return BIT_TEST_AND_SET(&rq->tail, 63);
  }
}

}  // namespace ppl
