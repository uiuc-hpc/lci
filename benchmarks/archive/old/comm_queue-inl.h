/**
 * @file comm_queue.cxx
 * @brief Methods for the communication and FEB queues.
 */

#include "comm_queue.h"
#include <stddef.h>

inline queue_t::queue_t()
{
  POOL = new queue_node_t[MAX_QUEUE_SIZE];
  head = &POOL[0];
  head->elem = NULL;
  head->next = NULL;
  tail = head;
}

inline queue_t::~queue_t() { delete[] POOL; }
inline void queue_t::init() { POOL_index = 1; }
inline void queue_t::enqueue_nosync(void* n)
{
  queue_node_t* node = &POOL[POOL_index];
  // This wraps the index when it exceeds MAX_QUEUE_SIZE.
  POOL_index = (POOL_index + 1) & (MAX_QUEUE_SIZE - 1);
  node->elem = n;
  node->next = NULL;
  tail->next = node;
  tail = node;
}

inline void queue_t::enqueue(void* n)
{
  int id = POOL_index.fetch_add(1);
  queue_node_t* node = &POOL[id & (MAX_QUEUE_SIZE - 1)];
  volatile queue_node_t* old_tail;
  queue_node_t* old_next;
  queue_node_t* ret;
  node->elem = n;
  node->next = NULL;
  while (1) {
    old_tail = tail;
    old_next = tail->next;
    if (old_tail == tail) {
      if (old_next != NULL) {
        __sync_bool_compare_and_swap(&tail, old_tail, old_next);
      } else {
        ret = static_cast<queue_node_t*>(
            __sync_val_compare_and_swap(&tail->next, old_next, node));
        if (ret == old_next) {
          __sync_bool_compare_and_swap(&tail, old_tail, node);
          break;
        }
      }
    }
  }
}

inline void* queue_t::dequeue()
{
  void* d = head->next->elem;
  head = head->next;
  return d;
}
