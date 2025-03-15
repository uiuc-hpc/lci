// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_MATCHING_ENGINE_MAP_HPP
#define LCI_MATCHING_ENGINE_MAP_HPP

namespace lci
{
class matching_engine_map_t : public matching_engine_impl_t
{
  static constexpr val_t VALUE_EMPTY = nullptr;
  static const int NODE_NUM_VALUES =
      (LCI_CACHE_LINE - sizeof(struct node_t*)) / sizeof(uint64_t);
  // static const uint32_t TABLE_BUCKET_WIDTH = LCI_CACHE_LINE /
  // sizeof(bucket_t) - 1;
  static const int BUCKET_NUM_QUEUES_DEFAULT = 3;
  static const int SLOT_NUM_VALUES = 2;
  static const int TABLE_BIT_SIZE = 16;
  static const int TABLE_NUM_MASTER_BUCKETS = 1 << 16;

  struct node_t {
    val_t values[NODE_NUM_VALUES];
    struct node_t* next;

    node_t() : next(nullptr)
    {
      for (int i = 0; i < NODE_NUM_VALUES; ++i) {
        values[i] = VALUE_EMPTY;
      }
    }
  };

  // 32 bytes
  struct queue_t {
    bool is_empty;
    bool is_queue;
    insert_type_t type;
    key_t key;
    // 16 bytes
    union {
      struct {
        node_t* head;
        node_t* tail;
      } list;
      val_t slots[SLOT_NUM_VALUES];
    };

    queue_t()
    {
      is_empty = true;
      is_queue = false;
      for (int i = 0; i < SLOT_NUM_VALUES; ++i) {
        slots[i] = VALUE_EMPTY;
      }
    }

    ~queue_t()
    {
      if (is_empty && is_queue) {
        node_t* node_p = list.head;
        node_t* node_q;
        while (node_p) {
          node_q = node_p;
          node_p = node_p->next;
          delete node_q;
        }
      }
    }

    val_t pop()
    {
      LCI_DBG_Assert(!is_empty, "This queue is empty!\n");
      val_t ret = VALUE_EMPTY;
      bool is_current_queue_nonempty = false;
      if (!is_queue) {
        for (int i = 0; i < SLOT_NUM_VALUES; ++i) {
          if (slots[i] != VALUE_EMPTY) {
            if (ret == VALUE_EMPTY) {
              ret = slots[i];
              slots[i] = VALUE_EMPTY;
            } else {
              is_current_queue_nonempty = true;
            }
          }
        }
        if (!is_current_queue_nonempty) {
          is_empty = true;
        }
        LCI_DBG_Assert(ret != VALUE_EMPTY,
                       "We cannot find one in the nonempty slots!\n");
      } else {
        size_t nlefts = 0;
        for (int i = 0; i < NODE_NUM_VALUES; ++i) {
          if (list.head->values[i] != VALUE_EMPTY) {
            if (ret == VALUE_EMPTY) {
              ret = list.head->values[i];
              list.head->values[i] = VALUE_EMPTY;
            } else {
              ++nlefts;
            }
          }
        }
        if (nlefts == 0) {
          node_t* node_p = list.head;
          list.head = list.head->next;
          delete node_p;
          if (list.head == nullptr) {
            is_empty = true;
          }
        } else if (nlefts <= SLOT_NUM_VALUES && list.head->next == nullptr) {
          // turn the linked node into slots
          int nvalues_idx = 0;
          val_t values[SLOT_NUM_VALUES];
          for (int i = 0; i < NODE_NUM_VALUES; ++i) {
            if (list.head->values[i] != VALUE_EMPTY) {
              values[nvalues_idx++] = list.head->values[i];
            }
          }
          delete list.head;
          memcpy(slots, values, nvalues_idx * sizeof(val_t));
          while (nvalues_idx < SLOT_NUM_VALUES) {
            slots[nvalues_idx++] = VALUE_EMPTY;
          }
          is_queue = false;
        }
        LCI_DBG_Assert(ret != VALUE_EMPTY,
                       "We cannot find one in the nonempty queue!\n");
      }
      return ret;
    }

    void push(val_t value)
    {
      LCI_DBG_Assert(!is_empty, "This queue is empty!\n");
      if (!is_queue) {
        if (slots[SLOT_NUM_VALUES - 1] != VALUE_EMPTY) {
          // turn slots into a linked node
          is_queue = true;
          node_t* new_node = new node_t();
          memcpy(new_node->values, slots, SLOT_NUM_VALUES * sizeof(val_t));
          new_node->values[SLOT_NUM_VALUES] = value;
          list.head = new_node;
          list.tail = new_node;
        } else {
          for (int i = 0; i < SLOT_NUM_VALUES; ++i) {
            if (slots[i] == VALUE_EMPTY) {
              slots[i] = value;
              break;
            }
          }
        }
      } else {
        if (list.tail->values[NODE_NUM_VALUES - 1] != VALUE_EMPTY) {
          // push this entry to a new linked node
          node_t* new_node = new node_t();
          new_node->values[0] = value;
          list.tail->next = new_node;
          list.tail = new_node;
        } else {
          // Find the first empty slot after a non-empty slot.
          int empty_slot = NODE_NUM_VALUES - 1;
          for (int i = NODE_NUM_VALUES - 1; i >= 0; --i) {
            if (list.tail->values[i] == VALUE_EMPTY) {
              empty_slot = i;
            } else {
              break;
            }
          }
          LCI_DBG_Assert(list.tail->values[empty_slot] == VALUE_EMPTY, "\n");
          list.tail->values[empty_slot] = value;
        }
      }
    }

    void setup(key_t key_, insert_type_t type_, val_t value)
    {
      LCI_DBG_Assert(is_empty, "This queue is nonn-empty!\n");
      is_empty = false;
      is_queue = false;
      key = key_;
      type = type_;
      slots[0] = value;
      for (int i = 1; i < SLOT_NUM_VALUES; ++i) {
        slots[i] = VALUE_EMPTY;
      }
    }
  };

  struct bucket_t {
    // 32 bytes
    struct {
      bucket_t* next = nullptr;
      spinlock_t lock;
      int nqueues;
      char padding[32 - sizeof(bucket_t*) - sizeof(spinlock_t) - sizeof(int)];
    } control;

    bucket_t() : bucket_t(BUCKET_NUM_QUEUES_DEFAULT) {}

    static bucket_t* alloc(int nqueues = BUCKET_NUM_QUEUES_DEFAULT)
    {
      static_assert(sizeof(bucket_t) == 32, "bucket_t size is not 32 bytes");
      size_t size = sizeof(bucket_t) + nqueues * sizeof(queue_t);
      bucket_t* bucket = (bucket_t*)alloc_memalign(size);
      bucket = new (bucket) bucket_t(nqueues);
      return bucket;
    }

    static void free(bucket_t* bucket)
    {
      bucket->~bucket_t();
      std::free(bucket);
    }

    static size_t size(int nqueues = BUCKET_NUM_QUEUES_DEFAULT)
    {
      return sizeof(bucket_t) + nqueues * sizeof(queue_t);
    }

    queue_t* get_queues_p()
    {
      return reinterpret_cast<queue_t*>(reinterpret_cast<char*>(this) +
                                        sizeof(bucket_t));
    }
    queue_t* get_queue_p(int i) { return &get_queues_p()[i]; }

    bucket_t(int nqueues_)
    {
      control.next = nullptr;
      control.nqueues = nqueues_;
      for (int i = 0; i < nqueues_; i++) {
        new (get_queue_p(i)) queue_t();
      }
    }

    ~bucket_t()
    {
      for (int i = 0; i < control.nqueues; i++) {
        get_queue_p(i)->~queue_t();
      }
    }
  };

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
            (((uint32_t)1 << TABLE_BIT_SIZE) - 1));
  }

 public:
  matching_engine_map_t(attr_t attr) : matching_engine_impl_t(attr)
  {
    static_assert(sizeof(bucket_t) == 32, "bucket_t size is not 16 bytes");
    static_assert(sizeof(node_t) == 64, "node_t size is not 16 bytes");
    table =
        (bucket_t*)alloc_memalign(TABLE_NUM_MASTER_BUCKETS * bucket_t::size());
    for (size_t i = 0; i < TABLE_NUM_MASTER_BUCKETS; i++) {
      new (get_master_bucket(i)) bucket_t();
    }
  }

  ~matching_engine_map_t()
  {
    for (size_t i = 0; i < TABLE_NUM_MASTER_BUCKETS; i++) {
      bucket_t* master = get_master_bucket(i);
      bucket_t* bucket_p = master;
      bucket_t* bucket_q;
      while (bucket_p) {
        bucket_q = bucket_p;
        bucket_p = bucket_p->control.next;
        if (bucket_q != master) {
          bucket_t::free(bucket_q);
        }
      }
      master->~bucket_t();
    }
    free(table);
  }

  bucket_t* get_master_bucket(uint32_t bucket_idx) const
  {
    return (bucket_t*)((char*)table + bucket_idx * bucket_t::size());
  }

  val_t insert(key_t key, val_t value, insert_type_t type) override
  {
    val_t ret = nullptr;
    const uint32_t bucket_idx = hash_fn(key);

    bucket_t* master = get_master_bucket(bucket_idx);
    insert_type_t target_type =
        type == insert_type_t::send ? insert_type_t::recv : insert_type_t::send;

    master->control.lock.lock();
    bucket_t* current_bucket = master;
    bucket_t* previous_bucket = nullptr;
    queue_t* target_queue = nullptr;
    queue_t* same_type_queue = nullptr;
    queue_t* first_empty_queue = nullptr;
    // Search the buckets and find the queue.
    while (current_bucket) {
      bool is_current_bucket_nonempty = false;
      for (int i = 0; i < current_bucket->control.nqueues; ++i) {
        queue_t* current_queue = current_bucket->get_queue_p(i);
        if (current_queue->is_empty) {
          if (first_empty_queue == nullptr) {
            first_empty_queue = current_queue;
            is_current_bucket_nonempty = true;
          }
          continue;
        } else {
          is_current_bucket_nonempty = true;
        }
        if (current_queue->key == key) {
          if (current_queue->type == target_type) {
            // found the right queue
            target_queue = current_queue;
          } else {
            // we would not find the right queue
            same_type_queue = current_queue;
          }
          is_current_bucket_nonempty = true;
          break;
        }
      }
      if (target_queue || same_type_queue) break;
      if (!is_current_bucket_nonempty && previous_bucket) {
        // remove current linked bucket
        previous_bucket->control.next = current_bucket->control.next;
        bucket_t::free(current_bucket);
        current_bucket = previous_bucket;
      }
      previous_bucket = current_bucket;
      current_bucket = current_bucket->control.next;
    }
    if (target_queue) {
      ret = target_queue->pop();
    } else if (same_type_queue) {
      // didn't find the target queue
      // Just append an entry to the queue
      same_type_queue->push(value);
    } else {
      if (!first_empty_queue) {
        // Didn't even find an empty queue in existing linked buckets.
        // Create a new bucket.
        int nqueues = previous_bucket->control.nqueues * 2 + 1;
        bucket_t* new_bucket = bucket_t::alloc(nqueues);
        LCI_DBG_Assert(current_bucket == nullptr, "\n");
        previous_bucket->control.next = new_bucket;
        first_empty_queue = new_bucket->get_queue_p(0);
      }
      // Create a queue from the empty queue.
      first_empty_queue->setup(key, type, value);
      // first_empty_queue->is_empty = false;
      // first_empty_queue->key = key;
      // first_empty_queue->type = type;
      // node_t* new_node = new node_t;
      // new_node->values[0] = value;
      // first_empty_queue->head = new_node;
      // first_empty_queue->tail = new_node;
    }
    master->control.lock.unlock();
    LCI_DBG_Log(LOG_TRACE, "matchtable", "insert (%lx, %p, %d) return %p\n",
                key, value, type, ret);
    return ret;
  }

 private:
  bucket_t* table;
};

}  // namespace lci

#endif  // LCI_MATCHING_ENGINE_MAP_HPP