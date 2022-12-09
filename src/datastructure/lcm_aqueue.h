#ifndef LCI_LCM_AQUEUE_H
#define LCI_LCM_AQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LCM_aqueue_entry_t {
  void* data;  // TODO: should this be volatile?
  char padding[LCI_CACHE_LINE - sizeof(void*)];
} LCM_aqueue_entry_t;

typedef struct LCM_aqueue_t {
  volatile uint64_t top;   // point to the next entry that is empty
  volatile uint64_t top2;  // point to the last entry that is full
  char padding0[LCI_CACHE_LINE - 2 * sizeof(uint64_t)];
  volatile uint64_t bot;   // point to the fist entry that is full
  volatile uint64_t bot2;  // point to the last entry that is empty
  char padding1[LCI_CACHE_LINE - 2 * sizeof(uint64_t)];
  uint64_t length;
  struct LCM_aqueue_entry_t* container;  // a pointer to type void*
} LCM_aqueue_t;

// The following functions are not thread-safe
static inline void LCM_aqueue_init(LCM_aqueue_t* queue, uint64_t capacity);
static inline void LCM_aqueue_fina(LCM_aqueue_t* queue);
// The following functions are thread-safe
static inline void LCM_aqueue_push(LCM_aqueue_t* queue, void* val);
static inline void* LCM_aqueue_pop(LCM_aqueue_t* queue);

#ifdef __cplusplus
}
#endif

static inline void LCM_aqueue_init(LCM_aqueue_t* queue, uint64_t capacity)
{
  LCM_Assert(sizeof(LCM_aqueue_entry_t) == LCI_CACHE_LINE,
             "Unexpected sizeof(LCM_aqueue_entry_t) %lu\n",
             sizeof(LCM_aqueue_entry_t));
  queue->container = LCIU_memalign(LCI_CACHE_LINE,
                                   (capacity + 1) * sizeof(LCM_aqueue_entry_t));
  queue->top = 0;
  queue->top2 = 0;
  queue->bot = 0;
  queue->bot2 = 0;
  queue->length = capacity + 1;
#ifdef LCI_DEBUG
  for (int i = 0; i < queue->length; ++i) {
    queue->container[i].data = NULL;
  }
#endif
}

static inline void LCM_aqueue_fina(LCM_aqueue_t* queue)
{
  LCIU_free(queue->container);
  queue->container = NULL;
  queue->top = 0;
  queue->top2 = 0;
  queue->bot = 0;
  queue->bot2 = 0;
  queue->length = 0;
}

static inline void LCM_aqueue_push(LCM_aqueue_t* queue, void* val)
{
  uint64_t current_bot2 = queue->bot2;
  // reserve a slot to write
  uint64_t current_top = __sync_fetch_and_add(&queue->top, 1);
  if (current_top - current_bot2 > queue->length - 1) {
    LCM_Assert(false, "The atomic queue is full! %lu - %lu > %lu\n",
               current_top, current_bot2, queue->length - 1);
  }
  // write to the slot
  //  __sync_synchronize();
  LCM_DBG_Assert(queue->container[current_top % queue->length].data == NULL,
                 "wrote to a nonempty value!\n");
  queue->container[current_top % queue->length].data = val;
  //  __sync_synchronize();
  // update top2 to tell the consumers they can safely read this slot.
  while (true) {
    uint64_t ret =
        __sync_val_compare_and_swap(&queue->top2, current_top, current_top + 1);
    if (ret == current_top) {
      // succeed!
      break;
    }
  }
}

static inline void* LCM_aqueue_pop(LCM_aqueue_t* queue)
{
  uint64_t current_top2 = queue->top2;
  uint64_t current_bot = queue->bot;
  if (current_top2 <= current_bot) {
    // the queue is empty
    return NULL;
  }
  //  LCM_DBG_Assert(current_top2 > current_bot, "bot %lu is ahead of top2
  //  %lu!\n", current_bot, current_top2);
  uint64_t ret =
      __sync_val_compare_and_swap(&queue->bot, current_bot, current_bot + 1);
  if (ret != current_bot) {
    // other thread is ahead of us
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
    uint64_t ret =
        __sync_val_compare_and_swap(&queue->bot2, current_bot, current_bot + 1);
    if (ret == current_bot) {
      // succeed!
      break;
    }
  }
  return result;
}

#endif  // LCI_LCM_AQUEUE_H
