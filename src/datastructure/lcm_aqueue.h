#ifndef LCI_LCM_AQUEUE_H
#define LCI_LCM_AQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LCM_aqueue_entry_t {
  void* data;
  LCIU_CACHE_PADDING(sizeof(void*));
} LCM_aqueue_entry_t;

typedef struct LCM_aqueue_t {
  atomic_uint_fast64_t top;   // point to the next entry that is empty
  atomic_uint_fast64_t top2;  // point to the last entry that is full
  LCIU_CACHE_PADDING(2 * sizeof(atomic_uint_fast64_t));
  atomic_uint_fast64_t bot;   // point to the fist entry that is full
  atomic_uint_fast64_t bot2;  // point to the last entry that is empty
  LCIU_CACHE_PADDING(2 * sizeof(atomic_uint_fast64_t));
  uint_fast64_t length;
  struct LCM_aqueue_entry_t* container;  // a pointer to type void*
} LCM_aqueue_t;

// The following functions are not thread-safe
static inline void LCM_aqueue_init(LCM_aqueue_t* queue, uint_fast64_t capacity);
static inline void LCM_aqueue_fina(LCM_aqueue_t* queue);
// The following functions are thread-safe
static inline void LCM_aqueue_push(LCM_aqueue_t* queue, void* val);
static inline void* LCM_aqueue_pop(LCM_aqueue_t* queue);

#ifdef __cplusplus
}
#endif

static inline void LCM_aqueue_init(LCM_aqueue_t* queue, uint_fast64_t capacity)
{
  LCM_Assert(sizeof(LCM_aqueue_entry_t) == LCI_CACHE_LINE,
             "Unexpected sizeof(LCM_aqueue_entry_t) %lu\n",
             sizeof(LCM_aqueue_entry_t));
  queue->container = LCIU_memalign(LCI_CACHE_LINE,
                                   (capacity + 1) * sizeof(LCM_aqueue_entry_t));
  atomic_init(&queue->top, 0);
  atomic_init(&queue->top2, 0);
  atomic_init(&queue->bot, 0);
  atomic_init(&queue->bot2, 0);
  queue->length = capacity + 1;
#ifdef LCI_DEBUG
  for (int i = 0; i < queue->length; ++i) {
    queue->container[i].data = NULL;
  }
#endif
  atomic_thread_fence(LCIU_memory_order_seq_cst);
}

static inline void LCM_aqueue_fina(LCM_aqueue_t* queue)
{
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  LCIU_free(queue->container);
  queue->container = NULL;
  atomic_init(&queue->top, 0);
  atomic_init(&queue->top2, 0);
  atomic_init(&queue->bot, 0);
  atomic_init(&queue->bot2, 0);
  queue->length = 0;
}

static inline void LCM_aqueue_push(LCM_aqueue_t* queue, void* val)
{
  uint_fast64_t current_bot2 =
      atomic_load_explicit(&queue->bot2, LCIU_memory_order_acquire);
  // reserve a slot to write
  uint_fast64_t current_top =
      atomic_fetch_add_explicit(&queue->top, 1, LCIU_memory_order_relaxed);
  if (current_top - current_bot2 > queue->length - 1) {
    LCM_Assert(false, "The atomic queue is full! %lu - %lu > %lu\n",
               current_top, current_bot2, queue->length - 1);
  }
  // write to the slot
  LCM_DBG_Assert(queue->container[current_top % queue->length].data == NULL,
                 "wrote to a nonempty value!\n");
  queue->container[current_top % queue->length].data = val;
  // update top2 to tell the consumers they can safely read this slot.
  while (true) {
    uint_fast64_t expected = current_top;
    _Bool succeed = atomic_compare_exchange_weak_explicit(
        &queue->top2, &expected, current_top + 1, LCIU_memory_order_release,
        LCIU_memory_order_relaxed);
    if (succeed) {
      // succeed!
      break;
    }
  }
}

static inline void* LCM_aqueue_pop(LCM_aqueue_t* queue)
{
  uint_fast64_t current_top2 =
      atomic_load_explicit(&queue->top2, LCIU_memory_order_acquire);
  uint_fast64_t current_bot =
      atomic_load_explicit(&queue->bot, LCIU_memory_order_relaxed);
  if (current_top2 <= current_bot) {
    // the queue is empty
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].lci_cq_pop_failed_empty++);
    return NULL;
  }
  //  LCM_DBG_Assert(current_top2 > current_bot, "bot %lu is ahead of top2
  //  %lu!\n", current_bot, current_top2);
  uint_fast64_t expected = current_bot;
  _Bool succeed = atomic_compare_exchange_strong_explicit(
      &queue->bot, &expected, current_bot + 1, LCIU_memory_order_relaxed,
      LCIU_memory_order_relaxed);
  if (!succeed) {
    // other thread is ahead of us
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].lci_cq_pop_failed_contention++);
    return NULL;
  }
  // we have successfully reserve an entry
  //  __sync_synchronize();
  void* result = queue->container[current_bot % queue->length].data;
#ifdef LCI_DEBUG
  queue->container[current_bot % queue->length].data = NULL;
#endif
  //  __sync_synchronize();
  // now that we got the value, we can update bot2 to tell the producers they
  // can safely write to this entry.
  while (true) {
    expected = current_bot;
    succeed = atomic_compare_exchange_weak_explicit(
        &queue->bot2, &expected, current_bot + 1, LCIU_memory_order_release,
        LCIU_memory_order_relaxed);
    if (succeed) {
      // succeed!
      break;
    }
  }
  LCII_PCOUNTERS_WRAPPER(
      LCII_pcounters[LCIU_get_thread_id()].lci_cq_pop_len_accumulated +=
      current_top2 - current_bot);
  LCII_PCOUNTERS_WRAPPER(
      LCII_pcounters[LCIU_get_thread_id()].lci_cq_pop_succeeded++);
  return result;
}

#endif  // LCI_LCM_AQUEUE_H
