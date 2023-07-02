#include "runtime/lcii.h"

#define QUEUE_KEY_EMPTY ((uint64_t)-1)
#define QUEUE_VAL_EMPTY ((uint64_t)-1)
#define NODE_VALUES_WIDTH \
  ((LCI_CACHE_LINE - sizeof(struct node_t*) - sizeof(int)) / sizeof(entry_t))

typedef struct entry_t {
  uint64_t key;
  uint64_t val;
} entry_t;

typedef struct node_t {
  entry_t entries[NODE_VALUES_WIDTH];
  struct node_t* next;
  int count;
} node_t;

static node_t* alloc_node()
{
  node_t* new_node = LCIU_malloc(sizeof(node_t));
  new_node->next = NULL;
  for (int i = 0; i < NODE_VALUES_WIDTH; ++i) {
    new_node->entries[i].key = QUEUE_KEY_EMPTY;
  }
  new_node->count = 0;
  return new_node;
}

typedef struct queue_t {
  node_t* head;
  node_t* tail;
} queue_t;

static void initialize_queue(queue_t* queue)
{
  queue->head = NULL;
  queue->tail = NULL;
}

static void clear_queue(queue_t* queue)
{
  node_t* node_p = queue->head;
  node_t* node_q;
  while (node_p) {
    node_q = node_p;
    node_p = node_p->next;
    LCIU_free(node_q);
  }
  queue->head = NULL;
  queue->tail = NULL;
}

static uint64_t search_queue(queue_t* queue, uint64_t key)
{
  uint64_t ret = QUEUE_VAL_EMPTY;
  node_t* current_node = queue->head;
  node_t* previous_node = NULL;
  while (current_node) {
    bool found = false;
    for (int i = 0; i < NODE_VALUES_WIDTH; ++i) {
      entry_t* current_entry = &current_node->entries[i];
      if (current_entry->key == key) {
        // found the entry!
        ret = current_entry->val;
        current_entry->key = QUEUE_KEY_EMPTY;
        --current_node->count;
        found = true;
        break;
      }
    }

    // Try to compress the list
    if (!previous_node && current_node->count == 0) {
      // remove this node (head node).
      LCI_DBG_Assert(current_node == queue->head, "%p != %p\n", queue->head,
                     current_node);
      queue->head = current_node->next;
      if (queue->head == NULL) queue->tail = NULL;
      LCIU_free(current_node);
      current_node = queue->head;
    } else if (previous_node && current_node->count + previous_node->count <=
                                    NODE_VALUES_WIDTH) {
      // compress two nodes
      int idx = 0;
      for (int i = 0; i < NODE_VALUES_WIDTH; ++i) {
        if (previous_node->entries[i].key != QUEUE_KEY_EMPTY) {
          if (idx != i) {
            previous_node->entries[idx] = previous_node->entries[i];
          }
          ++idx;
        }
      }
      for (int i = 0; i < NODE_VALUES_WIDTH; ++i) {
        if (current_node->entries[i].key != QUEUE_KEY_EMPTY) {
          previous_node->entries[idx] = current_node->entries[i];
          ++idx;
        }
      }
      for (; idx < NODE_VALUES_WIDTH; ++idx) {
        previous_node->entries[idx].key = QUEUE_KEY_EMPTY;
      }
      previous_node->count += current_node->count;
      // remove current node
      if (current_node == queue->tail) {
        queue->tail = previous_node;
      }
      previous_node->next = current_node->next;
      LCIU_free(current_node);
      current_node = previous_node;
    }

    if (found) {
      break;
    }
    previous_node = current_node;
    current_node = current_node->next;
  }
  return ret;
}

static void push_queue(queue_t* queue, uint64_t key, uint64_t val)
{
  if (queue->tail == NULL) {
    queue->head = alloc_node();
    queue->tail = queue->head;
    queue->tail->entries[0].key = key;
    queue->tail->entries[0].val = val;
    queue->tail->count = 1;
  } else {
    if (queue->tail->entries[NODE_VALUES_WIDTH - 1].key != QUEUE_KEY_EMPTY) {
      // push this entry to a new linked node
      node_t* new_node = alloc_node();
      new_node->entries[0].key = key;
      new_node->entries[0].val = val;
      new_node->count = 1;
      queue->tail->next = new_node;
      queue->tail = new_node;
    } else {
      // Find the first empty slot after a non-empty slot.
      int empty_slot = NODE_VALUES_WIDTH - 1;
      for (int i = NODE_VALUES_WIDTH - 1; i >= 0; --i) {
        if (queue->tail->entries[i].key == QUEUE_KEY_EMPTY) {
          empty_slot = i;
        } else {
          break;
        }
      }
      LCI_DBG_Assert(queue->tail->entries[empty_slot].key == QUEUE_KEY_EMPTY,
                     "\n");
      queue->tail->entries[empty_slot].key = key;
      queue->tail->entries[empty_slot].val = val;
      ++queue->tail->count;
    }
  }
}

typedef struct table_t {
  queue_t send_queue;
  queue_t recv_queue;
  LCIU_spinlock_t lock;
} table_t;

static void LCII_matchtable_queue_create(LCI_matchtable_t* table_p)
{
  table_t* table = LCIU_malloc(sizeof(table_t));
  LCIU_spinlock_init(&table->lock);
  initialize_queue(&table->send_queue);
  initialize_queue(&table->recv_queue);
  *table_p = (LCI_matchtable_t)table;
}

static void LCII_matchtable_queue_free(LCI_matchtable_t* mt_p)
{
  table_t* table = *(table_t**)mt_p;
  clear_queue(&table->send_queue);
  clear_queue(&table->recv_queue);
  LCIU_spinlock_fina(&table->lock);
  LCIU_free(table);
  *mt_p = NULL;
}

static LCI_error_t LCII_matchtable_queue_insert(
    LCI_matchtable_t mt, uint64_t key, uint64_t* val,
    enum LCII_matchtable_insert_type type)
{
  table_t* table = (table_t*)mt;
  LCI_error_t ret = LCI_ERR_FATAL;

  LCIU_acquire_spinlock(&table->lock);
  if (type == LCII_MATCHTABLE_SEND) {
    // Search the posted recv queue for a matching entry
    uint64_t result = search_queue(&table->send_queue, key);
    if (result == QUEUE_VAL_EMPTY) {
      // Did not find a matching unexpected send.
      push_queue(&table->recv_queue, key, *val);
      ret = LCI_ERR_RETRY;
    } else {
      *val = result;
      ret = LCI_OK;
    }
  } else {
    // Search the posted recv queue for a matching entry
    uint64_t result = search_queue(&table->recv_queue, key);
    if (result == QUEUE_VAL_EMPTY) {
      // Did not find a matching posted recv.
      push_queue(&table->send_queue, key, *val);
      ret = LCI_ERR_RETRY;
    } else {
      *val = result;
      ret = LCI_OK;
    }
  }
  LCI_DBG_Log(LCI_LOG_TRACE, "matchtable", "insert (%lx, %p, %d), return %d\n",
              key, val, type, ret);
  LCIU_release_spinlock(&table->lock);
  return ret;
}

void LCII_matchtable_queue_setup_ops(struct LCII_matchtable_ops_t* ops)
{
  ops->create = LCII_matchtable_queue_create;
  ops->free = LCII_matchtable_queue_free;
  ops->insert = LCII_matchtable_queue_insert;
}
