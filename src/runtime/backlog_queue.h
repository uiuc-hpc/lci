#ifndef LCI_BACKLOG_QUEUE_H
#define LCI_BACKLOG_QUEUE_H

typedef enum {
  LCII_BQ_SENDS,
  LCII_BQ_SEND,
  LCII_BQ_PUT,
  LCII_BQ_PUTIMM,
} LCII_bqe_type_t;

// backlog queue entry, 96 B
// TODO: should further reduce to one cache line
typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCII_bq_entry_t {
  void* buf;                 // 8B
  size_t size;               // 8B
  LCII_bqe_type_t bqe_type;  // 4B
  int rank;                  // 4B
  LCIS_mr_t mr;              // 24B
  void* ctx;                 // 8B
  LCIS_meta_t meta;          // 4B
  // only needed by put
  LCIS_offset_t offset;          // 8B
  uintptr_t base;                // 8B
  LCIS_rkey_t rkey;              // 8B
  struct LCII_bq_entry_t* next;  // 8B
} LCII_bq_entry_t;

typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCII_backlog_queue_t {
  int length;
  LCII_bq_entry_t* head;
  LCII_bq_entry_t* tail;
} LCII_backlog_queue_t;

static inline void LCII_bq_init(LCII_backlog_queue_t* bq_p)
{
  bq_p->length = 0;
  bq_p->head = NULL;
  bq_p->tail = NULL;
}

static inline void LCII_bq_fini(LCII_backlog_queue_t* bq_p)
{
  if (bq_p->length != 0) {
    LCM_Warn("There are still %d pending entries in the backlog queue\n",
             bq_p->length);
    LCII_bq_entry_t* p = bq_p->head;
    LCII_bq_entry_t* q;
    while (p != NULL) {
      q = p;
      p = p->next;
      LCIU_free(q);
      --bq_p->length;
    }
    bq_p->head = NULL;
    bq_p->tail = NULL;
    LCM_Assert(bq_p->length == 0, "backlog queue is in an incosistent state\n");
  }
}

// this can be called by both the worker threads and progress threads
static inline void LCII_bq_push(LCII_backlog_queue_t* bq_p,
                                LCII_bq_entry_t* entry)
{
  ++bq_p->length;
#ifdef LCI_USE_PERFORMANCE_COUNTER
  LCII_pcounters[LCIU_get_thread_id()].backlog_queue_total_count++;
  LCIU_MAX_ASSIGN(LCII_pcounters[LCIU_get_thread_id()].backlog_queue_max_len,
                  bq_p->length);
#endif
  entry->next = NULL;
  if (bq_p->head == NULL) {
    bq_p->head = entry;
  } else {
    bq_p->tail->next = entry;
  }
  bq_p->tail = entry;
}

// this could be called by all threads
static inline int LCII_bq_is_empty(LCII_backlog_queue_t* bq_p)
{
  return bq_p->length == 0;
}

// this should only be called by one progress threads
static inline LCII_bq_entry_t* LCII_bq_top(LCII_backlog_queue_t* bq_p)
{
  return bq_p->head;
}

// this should only be called by one progress threads
static inline LCII_bq_entry_t* LCII_bq_pop(LCII_backlog_queue_t* bq_p)
{
  if (bq_p->length == 0) {
    return NULL;
  }
  --bq_p->length;
  LCII_bq_entry_t* ret = bq_p->head;
  if (bq_p->length == 0) {
    bq_p->tail = NULL;
  }
  bq_p->head = bq_p->head->next;
  ret->next = NULL;
  return ret;
}

#endif  // LCI_BACKLOG_QUEUE_H
