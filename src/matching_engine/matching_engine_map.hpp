#ifndef LCI_MATCHING_ENGINE_MAP_HPP
#define LCI_MATCHING_ENGINE_MAP_HPP

namespace lci
{
class matching_engine_map_t : public matching_engine_impl_t
{
  static constexpr val_t VALUE_EMPTY = nullptr;
  static const int NODE_NUM_VALUES = (LCI_CACHE_LINE - sizeof(struct node_t*)) / sizeof(uint64_t);
  // static const uint32_t TABLE_BUCKET_WIDTH = LCI_CACHE_LINE / sizeof(bucket_t) - 1;
  static const int BUCKET_NUM_QUEUES = 3;
  static const int SLOT_NUM_VALUES = 2;
  static const int TABLE_BIT_SIZE = 16;
  static const int TABLE_NUM_BUCKETS = 1 << 16;


  struct node_t {
    val_t values[NODE_NUM_VALUES];
    struct node_t* next;
    
    node_t() : next(nullptr) {
      for (int i = 0; i < NODE_NUM_VALUES; ++i) {
        values[i] = VALUE_EMPTY;
      }
    }
  };
  
  // 32 bytes
  struct queue_t {
    bool is_empty = true;
    bool is_queue;
    insert_type_t type;
    key_t key;
    union {
      struct {
        node_t* head;
        node_t* tail;
      };
      val_t values[SLOT_NUM_VALUES]; // TODO: use this field to optimize the search
    };
    queue_t() {
      is_queue = false;
      for (int i = 0; i < SLOT_NUM_VALUES; ++i) {
        values[i] = VALUE_EMPTY;
      }
    }
  };

  struct bucket_t {
    // 32 bytes
    struct {
      bucket_t* next = nullptr;
      spinlock_t lock;
      int nqueues; // TODO: use this field to optimize the search
      char padding[32 - sizeof(bucket_t*) - sizeof(spinlock_t) - sizeof(int)];
    } control;
    queue_t queues[BUCKET_NUM_QUEUES];

    bucket_t() {
      control.next = nullptr;
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
            ((uint32_t)1 << TABLE_BIT_SIZE) - 1);
  }

 public:
  matching_engine_map_t(attr_t attr) : matching_engine_impl_t(attr) {
    // static_assert(sizeof(bucket_t) == 16, "bucket_t size is not 16 bytes");
    // static_assert(sizeof(node_t) == 16, "node_t size is not 16 bytes");
    table = new bucket_t[TABLE_NUM_BUCKETS];
  }

  ~matching_engine_map_t() {
    for (size_t i = 0; i < TABLE_NUM_BUCKETS; i++) {
      bucket_t* bucket_p = &table[i];
      bucket_t* bucket_q;
      while (bucket_p) {
        for (auto slot : bucket_p->queues) {
          if (slot.is_queue) {
            node_t* node_p = slot.head;
            node_t* node_q;
            while (node_p) {
              node_q = node_p;
              node_p = node_p->next;
              delete node_q;
            }
          }
        }
        bucket_q = bucket_p;
        bucket_p = bucket_p->control.next;
        if (bucket_q != &table[i]) {
          delete bucket_q;
        }
      }
    }
    delete[] table;
  }
  
  val_t insert(key_t key, val_t value, insert_type_t type) override
  {
    val_t ret = nullptr;
    const uint32_t bucket_idx = hash_fn(key);

    bucket_t* master = &table[bucket_idx];
    insert_type_t target_type = type == insert_type_t::send ? insert_type_t::recv : insert_type_t::send;

    master->control.lock.lock();
    bucket_t* current_bucket = master;
    bucket_t* previous_bucket = nullptr;
    queue_t* target_queue = nullptr;
    queue_t* same_type_queue = nullptr;
    queue_t* first_empty_queue = nullptr;
    // Search the buckets and find the queue.
    while (current_bucket) {
      bool is_current_bucket_nonempty = false;
      for (int i = 0; i < BUCKET_NUM_QUEUES; ++i) {
        queue_t* current_queue = &current_bucket->queues[i];
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
        delete current_bucket;
        current_bucket = previous_bucket;
      }
      previous_bucket = current_bucket;
      current_bucket = current_bucket->control.next;
    }
    if (target_queue) {
      // find the target queue
      bool is_current_queue_nonempty = false;
      for (int i = 0; i < NODE_NUM_VALUES; ++i) {
        if (target_queue->head->values[i] != VALUE_EMPTY) {
          if (ret == VALUE_EMPTY) {
            ret = target_queue->head->values[i];
            target_queue->head->values[i] = VALUE_EMPTY;
          } else {
            is_current_queue_nonempty = true;
          }
        }
      }
      // We are surely going to find one in the header node
      LCI_DBG_Assert(ret != VALUE_EMPTY, "This header node is empty!\n");
      if (!is_current_queue_nonempty) {
        // Remove the head node
        node_t* tmp = target_queue->head;
        target_queue->head = target_queue->head->next;
        if (target_queue->head == nullptr) {
          LCI_DBG_Assert(target_queue->tail == tmp, "\n");
          target_queue->tail = nullptr;
          target_queue->is_empty = true;
        }
        delete tmp;
      }
    } else {
      // didn't find the target queue
      if (same_type_queue) {
        // Just append an entry to the queue
        LCI_DBG_Assert(same_type_queue->head && same_type_queue->tail, "This queue is empty!\n");
        if (same_type_queue->tail->values[NODE_NUM_VALUES - 1] !=
            VALUE_EMPTY) {
          // push this entry to a new linked node
          node_t* new_node = new node_t();
          new_node->values[0] = value;
          same_type_queue->tail->next = new_node;
          same_type_queue->tail = new_node;
        } else {
          // Find the first empty slot after a non-empty slot.
          int empty_slot = NODE_NUM_VALUES - 1;
          for (int i = NODE_NUM_VALUES - 1; i >= 0; --i) {
            if (same_type_queue->tail->values[i] == VALUE_EMPTY) {
              empty_slot = i;
            } else {
              break;
            }
          }
          LCI_DBG_Assert(
              same_type_queue->tail->values[empty_slot] == VALUE_EMPTY, "\n");
          same_type_queue->tail->values[empty_slot] = value;
        }
      } else {
        if (!first_empty_queue) {
          // Didn't even find an empty queue in existing linked buckets.
          // Create a new bucket.
          bucket_t* new_bucket = new bucket_t;
          LCI_DBG_Assert(current_bucket == nullptr, "\n");
          previous_bucket->control.next = new_bucket;
          first_empty_queue = &new_bucket->queues[0];
        }
        // Create a queue from the empty queue.
        first_empty_queue->is_empty = false;
        first_empty_queue->key = key;
        first_empty_queue->type = type;
        node_t* new_node = new node_t;
        new_node->values[0] = value;
        first_empty_queue->head = new_node;
        first_empty_queue->tail = new_node;
      }
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