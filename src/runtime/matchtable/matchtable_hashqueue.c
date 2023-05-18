#include "runtime/lcii.h"

#define TABLE_TAG_EMPTY ((uint64_t)-1)
#define QUEUE_VAL_EMPTY ((uint64_t)-1)
#define NODE_VALUES_WIDTH \
  ((LCI_CACHE_LINE - sizeof(struct node_t*)) / sizeof(uint64_t))
#define TABLE_BUCKET_WIDTH (LCI_CACHE_LINE / sizeof(queue_t) - 1)

int TABLE_BIT_SIZE = 16;
int TABLE_NUM_BUCKETS = 1 << 16;

typedef struct node_t {
  uint64_t values[NODE_VALUES_WIDTH];
  struct node_t* next;
} node_t;

static node_t* alloc_node()
{
  node_t* new_node = LCIU_malloc(sizeof(node_t));
  new_node->next = NULL;
  for (int i = 0; i < NODE_VALUES_WIDTH; ++i) {
    new_node->values[i] = QUEUE_VAL_EMPTY;
  }
  return new_node;
}

typedef struct queue_t {
  uint64_t tag;
  node_t* head;
  node_t* tail;
  enum LCII_matchtable_insert_type type;
} queue_t;

typedef struct bucket_t {
  struct {
    struct bucket_t* next;
    LCIU_spinlock_t lock;
    char padding[sizeof(queue_t) - sizeof(LCIU_spinlock_t) -
                 sizeof(struct bucket_t*)];
  } control;
  queue_t queues[TABLE_BUCKET_WIDTH];
} bucket_t;

static void initialize_bucket(bucket_t* new_bucket)
{
  LCIU_spinlock_init(&new_bucket->control.lock);
  new_bucket->control.next = NULL;
  for (int j = 0; j < TABLE_BUCKET_WIDTH; j++) {
    new_bucket->queues[j].tag = TABLE_TAG_EMPTY;
    new_bucket->queues[j].head = NULL;
    new_bucket->queues[j].tail = NULL;
    new_bucket->queues[j].type = 0;
  }
}

static inline uint32_t hash_fn(const uint64_t k)
{
  // default values recommended by http://isthe.com/chongo/tech/comp/fnv/
  static const uint32_t Prime = 0x01000193;  //   16777619
  static const uint32_t Seed = 0x811C9DC5;   // 2166136261

  uint32_t hash = ((k & 0xff) ^ Seed) * Prime;
  hash = (((k >> 8) & 0xff) ^ hash) * Prime;
  hash = (((k >> 16) & 0xff) ^ hash) * Prime;
  hash = (((k >> 24) & 0xff) ^ hash) * Prime;
  hash = (((k >> 32) & 0xff) ^ hash) * Prime;
  hash = (((k >> 40) & 0xff) ^ hash) * Prime;
  hash = (((k >> 48) & 0xff) ^ hash) * Prime;
  hash = (((k >> 56) & 0xff) ^ hash) * Prime;

  // Mask into smaller space.
  return (((hash >> TABLE_BIT_SIZE) ^ hash) &
          ((uint32_t)1 << TABLE_BIT_SIZE) - 1);
}

static inline bucket_t* create_table(size_t num_buckets)
{
  bucket_t* table = LCIU_malloc(num_buckets * sizeof(bucket_t));

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < num_buckets; i++) {
    initialize_bucket(&table[i]);
  }
  return table;
}

static void LCII_matchtable_hashqueue_free(LCI_matchtable_t* mt_p)
{
  bucket_t* table = *(bucket_t**)mt_p;
  int num_buckets = 1 << TABLE_BIT_SIZE;
  for (size_t i = 0; i < num_buckets; i++) {
    bucket_t* bucket_p = &table[i];
    bucket_t* bucket_q;
    while (bucket_p) {
      LCIU_spinlock_fina(&table[i].control.lock);
      for (int j = 0; j < TABLE_BUCKET_WIDTH; j++) {
        node_t* node_p = table[i].queues[j].head;
        node_t* node_q;
        while (node_p) {
          node_q = node_p;
          node_p = node_p->next;
          LCIU_free(node_q);
        }
      }
      bucket_q = bucket_p;
      bucket_p = bucket_p->control.next;
      if (bucket_q != &table[i]) {
        LCIU_free(bucket_q);
      }
    }
  }
  LCIU_free(table);
  *mt_p = NULL;
}

static LCI_error_t LCII_matchtable_hashqueue_insert(
    LCI_matchtable_t mt, uint64_t key, uint64_t* val,
    enum LCII_matchtable_insert_type type)
{
  bucket_t* table = (bucket_t*)mt;

  LCI_error_t ret = LCI_ERR_FATAL;
  const uint32_t bucket_idx = hash_fn(key);

  bucket_t* master = &table[bucket_idx];
  uint64_t target_type = 1 - type;

  LCIU_acquire_spinlock(&master->control.lock);
  bucket_t* current_bucket = &table[bucket_idx];
  bucket_t* previous_bucket = NULL;
  queue_t* target_queue = NULL;
  queue_t* same_type_queue = NULL;
  queue_t* empty_queue = NULL;
  // Search the buckets and find the queue.
  while (current_bucket) {
    bool at_least_one_nonempty = false;
    for (int i = 0; i < TABLE_BUCKET_WIDTH; ++i) {
      queue_t* current_queue = &current_bucket->queues[i];
      if (current_queue->tag == key) {
        if (current_queue->type == target_type) {
          // found the right queue
          target_queue = current_queue;
        } else {
          // we would not find the right queue
          same_type_queue = current_queue;
        }
        at_least_one_nonempty = true;
        break;
      } else if (current_queue->tag == TABLE_TAG_EMPTY) {
        // find an empty queue
        if (empty_queue == NULL) {
          empty_queue = current_queue;
          at_least_one_nonempty = true;
        }
      } else {
        at_least_one_nonempty = true;
      }
    }
    if (target_queue || same_type_queue) break;
    if (!at_least_one_nonempty && previous_bucket) {
      // remove current linked bucket
      previous_bucket->control.next = current_bucket->control.next;
      LCIU_free(current_bucket);
      current_bucket = previous_bucket;
    }
    previous_bucket = current_bucket;
    current_bucket = current_bucket->control.next;
  }
  if (target_queue) {
    // find the target queue
    bool at_least_one_nonempty = false;
    for (int i = 0; i < NODE_VALUES_WIDTH; ++i) {
      if (target_queue->head->values[i] != QUEUE_VAL_EMPTY) {
        if (ret != LCI_OK) {
          *val = target_queue->head->values[i];
          target_queue->head->values[i] = QUEUE_VAL_EMPTY;
          ret = LCI_OK;
        } else {
          at_least_one_nonempty = true;
        }
      }
    }
    LCM_DBG_Assert(ret == LCI_OK, "This header node is empty!\n");
    if (!at_least_one_nonempty) {
      // Remove the head node
      node_t* tmp = target_queue->head;
      target_queue->head = target_queue->head->next;
      if (target_queue->head == NULL) {
        LCM_DBG_Assert(target_queue->tail == tmp, "\n");
        target_queue->tail = NULL;
        target_queue->tag = TABLE_TAG_EMPTY;
      }
      LCIU_free(tmp);
    }
  } else {
    // didn't find the target queue
    if (same_type_queue) {
      // Just append an entry to the queue
      LCM_DBG_Assert(same_type_queue->head && same_type_queue->tail, "\n");
      if (same_type_queue->tail->values[NODE_VALUES_WIDTH - 1] !=
          QUEUE_VAL_EMPTY) {
        // push this entry to a new linked node
        node_t* new_node = alloc_node();
        new_node->values[0] = *val;
        same_type_queue->tail->next = new_node;
        same_type_queue->tail = new_node;
      } else {
        // Find the first empty slot after a non-empty slot.
        int empty_slot = NODE_VALUES_WIDTH - 1;
        for (int i = NODE_VALUES_WIDTH - 1; i >= 0; --i) {
          if (same_type_queue->tail->values[i] == QUEUE_VAL_EMPTY) {
            empty_slot = i;
          } else {
            break;
          }
        }
        LCM_DBG_Assert(
            same_type_queue->tail->values[empty_slot] == QUEUE_VAL_EMPTY, "\n");
        same_type_queue->tail->values[empty_slot] = *val;
      }
    } else {
      if (!empty_queue) {
        // Didn't even find an empty queue in existing linked buckets.
        // Create a new bucket.
        bucket_t* new_bucket = LCIU_malloc(sizeof(bucket_t));
        initialize_bucket(new_bucket);
        LCM_DBG_Assert(current_bucket == NULL, "\n");
        previous_bucket->control.next = new_bucket;
        empty_queue = &new_bucket->queues[0];
      }
      // Create a queue from the empty queue.
      empty_queue->tag = key;
      empty_queue->type = type;
      node_t* new_node = alloc_node();
      new_node->values[0] = *val;
      empty_queue->head = new_node;
      empty_queue->tail = new_node;
    }
    ret = LCI_ERR_RETRY;
  }
  LCM_DBG_Log(LCM_LOG_DEBUG, "matchtable", "insert (%lx, %p, %d) return %d\n",
              key, val, type, ret);
  LCIU_release_spinlock(&master->control.lock);
  LCM_DBG_Assert(ret != LCI_ERR_FATAL, "Unexpected return value!\n");
  return ret;
}

static void LCII_matchtable_hashqueue_create(LCI_matchtable_t* table_p)
{
  int num_buckets = 1 << TABLE_BIT_SIZE;
  bucket_t* table = create_table(num_buckets);
  *table_p = (LCI_matchtable_t)table;
}

void LCII_matchtable_hashqueue_setup_ops(struct LCII_matchtable_ops_t* ops)
{
  ops->create = LCII_matchtable_hashqueue_create;
  ops->free = LCII_matchtable_hashqueue_free;
  ops->insert = LCII_matchtable_hashqueue_insert;
}
