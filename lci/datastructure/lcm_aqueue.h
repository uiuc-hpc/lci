#ifndef LCI_LCM_AQUEUE_H
#define LCI_LCM_AQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LCM_aqueue_entry_t {
  void* data;
  atomic_uint_fast64_t tag;
  LCIU_CACHE_PADDING(sizeof(void*) + sizeof(atomic_uint_fast64_t));
} LCM_aqueue_entry_t;

typedef struct LCM_aqueue_t {
  atomic_uint_fast64_t top;  // point to the next entry that is empty
  LCIU_CACHE_PADDING(sizeof(atomic_uint_fast64_t));
  atomic_uint_fast64_t bot;  // point to the fist entry that is full
  LCIU_CACHE_PADDING(sizeof(atomic_uint_fast64_t));
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
  LCI_Assert(sizeof(LCM_aqueue_entry_t) == LCI_CACHE_LINE,
             "Unexpected sizeof(LCM_aqueue_entry_t) %lu\n",
             sizeof(LCM_aqueue_entry_t));
  queue->container = LCIU_memalign(LCI_CACHE_LINE,
                                   (capacity + 1) * sizeof(LCM_aqueue_entry_t));
  atomic_init(&queue->top, 0);
  atomic_init(&queue->bot, 0);
  queue->length = capacity + 1;
  for (int i = 0; i < queue->length; ++i) {
    queue->container[i].data = 0;
    atomic_init(&queue->container[i].tag, -1);
  }
  atomic_thread_fence(LCIU_memory_order_seq_cst);
}

static inline void LCM_aqueue_fina(LCM_aqueue_t* queue)
{
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  LCIU_free(queue->container);
  queue->container = NULL;
  atomic_init(&queue->top, 0);
  atomic_init(&queue->bot, 0);
  queue->length = 0;
}

static inline void LCM_aqueue_push(LCM_aqueue_t* queue, void* val)
{
  LCT_time_t time0 = LCT_now();
  LCII_PCOUNTER_STARTT(cq_push_internal, time0);
  // reserve a slot to write
  LCII_PCOUNTER_STARTT(cq_push_faa, time0);
  uint_fast64_t current_top =
      atomic_fetch_add_explicit(&queue->top, 1, LCIU_memory_order_relaxed);
  LCT_time_t time1 = LCT_now();
  LCII_PCOUNTER_ENDT(cq_push_faa, time1);
  // write to the slot
  LCII_PCOUNTER_STARTT(cq_push_write, time1);
  LCI_DBG_Assert(atomic_load_explicit(
                     &queue->container[current_top % queue->length].tag,
                     LCIU_memory_order_acquire) != current_top - queue->length,
                 "wrote to a nonempty value!\n");
  queue->container[current_top % queue->length].data = val;
  LCT_time_t time2 = LCT_now();
  LCII_PCOUNTER_ENDT(cq_push_write, time2);
  // update top2 to tell the consumers they can safely read this slot.
  LCII_PCOUNTER_STARTT(cq_push_store, time2);
  atomic_store_explicit(&queue->container[current_top % queue->length].tag,
                        current_top, LCIU_memory_order_release);
  LCT_time_t time3 = LCT_now();
  LCII_PCOUNTER_ENDT(cq_push_store, time3);
  LCII_PCOUNTER_ENDT(cq_push_internal, time3);
}

static inline void* LCM_aqueue_pop(LCM_aqueue_t* queue)
{
  uint_fast64_t current_bot =
      atomic_load_explicit(&queue->bot, LCIU_memory_order_relaxed);
  if (atomic_load_explicit(&queue->container[current_bot % queue->length].tag,
                           LCIU_memory_order_acquire) != current_bot) {
    // the queue is empty
    return NULL;
  }
  current_bot =
      atomic_fetch_add_explicit(&queue->bot, 1, LCIU_memory_order_relaxed);
  while (
      atomic_load_explicit(&queue->container[current_bot % queue->length].tag,
                           LCIU_memory_order_acquire) != current_bot) {
    // some thread is ahead of us. We got a cell that is empty.
    uint_fast64_t expected = current_bot + 1;
    _Bool succeed = atomic_compare_exchange_weak_explicit(
        &queue->bot, &expected, current_bot, LCIU_memory_order_relaxed,
        LCIU_memory_order_relaxed);
    if (succeed) return NULL;
  }
  LCII_PCOUNTER_START(cq_pop_timer);
  // we have successfully reserve an entry
  void* result = queue->container[current_bot % queue->length].data;
#ifdef LCI_DEBUG
  atomic_store_explicit(&queue->container[current_bot % queue->length].tag,
                        current_bot + 1, LCIU_memory_order_release);
#endif
  LCII_PCOUNTER_END(cq_pop_timer);
  return result;
}

#endif  // LCI_LCM_AQUEUE_H