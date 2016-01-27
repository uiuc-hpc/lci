/**
 * @file comm_queue.h
 * @brief Class for the communication and FEB queues.
 */

#ifndef COMM_QUEUE_H_
#define COMM_QUEUE_H_

#include <stddef.h>

/**
 * Maximum size for a queue, must be a power of 2.
 */
#define MAX_QUEUE_SIZE 4096

#if defined(__GNUC__)
/** Force a function to be inlined. */
#define FORCE_INLINE __attribute__((always_inline))
#else
#define FORCE_INLINE inline
#endif

/**
 * Queue used for communication requests, wait requests, and fill requests
 */
class queue_t {
 public:
  /**
   * Create an empty queue.
   * @todo Find out whether we need to allocate this much at the start.
   */
  queue_t();

  /**
   * Delete this queue.
   */
  ~queue_t();

  /**
   * Initialize the queue
   */
  void init();

  /**
   * Enqueues an item into a queue (not thread safe).
   * @param q Queue to insert item into.
   * @param n Item to insert into queue.
   * @todo Test if inlining is beneficial
   */
  void enqueue_nosync(void* n);

  /**
   * Enqueues an item into a queue (thread safe).
   * @param q Queue to insert item into.
   * @param n Item to insert into queue.
   * @todo Test if inlining is beneficial
   */
  void enqueue(void* n);

  /**
   * Dequeues an item from a queue (not thread safe)
   * @param q Queue to remove item from.
   * @param d Item to remove from queue.
   * @todo Test if inlining is beneficial
   */
  void* dequeue();

  /**
   * Checks if a queue is empty
   * @param q Queue to check.
   */
  inline bool empty() { return head->next == NULL; }

 private:
  /** Node in the queue. */
  struct queue_node_t {
    /** Data in this node. */
    void* elem;
    /** Next node in the queue. */
    queue_node_t* next;
  };

  /** Head of the queue. */
  volatile queue_node_t* head;
  /** Tail of the queue. */
  volatile queue_node_t* tail;
  /** Always points to the start of the queue. */
  queue_node_t* POOL;
  /** Current index into the pool. */
  std::atomic<int> POOL_index;
};

#include "comm_queue-inl.h"

template <typename T>
class mpsc_queue_t {
 public:
  mpsc_queue_t()
      : _head(reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t)),
        _tail(_head.load(std::memory_order_relaxed)) {
    buffer_node_t* front = _head.load(std::memory_order_relaxed);
    front->next.store(NULL, std::memory_order_relaxed);
  }

  ~mpsc_queue_t() {
    T output;
    while (this->dequeue(output)) {
    }
    buffer_node_t* front = _head.load(std::memory_order_relaxed);
    delete front;
  }

  void enqueue(const T& input) {
    buffer_node_t* node =
        reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t);
    node->data = input;
    node->next.store(NULL, std::memory_order_relaxed);

    buffer_node_t* prev_head = _head.exchange(node, std::memory_order_acq_rel);
    prev_head->next.store(node, std::memory_order_release);
  }

  bool dequeue(T& output) {
    buffer_node_t* tail = _tail.load(std::memory_order_relaxed);
    buffer_node_t* next = tail->next.load(std::memory_order_acquire);

    if (next == NULL) {
      return false;
    }

    output = next->data;
    _tail.store(next, std::memory_order_release);
    delete tail;
    return true;
  }

 private:
  struct buffer_node_t {
    T data;
    std::atomic<buffer_node_t*> next;
  };

  typedef typename std::aligned_storage<
      sizeof(buffer_node_t), std::alignment_of<buffer_node_t>::value>::type
      buffer_node_aligned_t;

  std::atomic<buffer_node_t*> _head;
  std::atomic<buffer_node_t*> _tail;

  mpsc_queue_t(const mpsc_queue_t&) {}
  void operator=(const mpsc_queue_t&) {}
};

#endif  // COMM_QUEUE_H_
