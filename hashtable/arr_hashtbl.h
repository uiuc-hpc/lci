#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_

#include <mutex>
#include <atomic>
#include <stdexcept>
#include "hashtbl.h"
#include "config.h"

static const uint64_t EMPTY = (uint64_t)-1;
#ifndef CONFIG_TABLE_SIZE
static const int TBL_BIT_SIZE = 8;
#else
static const int TBL_BIT_SIZE = CONFIG_TABLE_SIZE;
#endif
static const int TBL_WIDTH = 4;

// static_assert((1 << TBL_BIT_SIZE) >= 4 * MAX_CONCURRENCY,
//              "Hash table is not large enough");

// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
static const uint32_t Prime = 0x01000193;  //   16777619
static const uint32_t Seed = 0x811C9DC5;   // 2166136261
#define TINY_MASK(x) (((u_int32_t)1 << (x)) - 1)
#define FNV1_32_INIT ((u_int32_t)2166136261)

inline uint32_t myhash(const uint64_t k) {
  uint32_t hash = ((k & 0xff) ^ Seed) * Prime;
  hash = (((k >> 8) & 0xff) ^ hash) * Prime;
  hash = (((k >> 16) & 0xff) ^ hash) * Prime;
  hash = (((k >> 24) & 0xff) ^ hash) * Prime;
  hash = (((k >> 32) & 0xff) ^ hash) * Prime;
  hash = (((k >> 40) & 0xff) ^ hash) * Prime;
  hash = (((k >> 48) & 0xff) ^ hash) * Prime;
  hash = (((k >> 56) & 0xff) ^ hash) * Prime;

  // Mask into smaller space.
  return (((hash >> TBL_BIT_SIZE) ^ hash) & TINY_MASK(TBL_BIT_SIZE));
}

struct hash_val {
  union {
    struct {
      uint64_t tag;
      void* val;
    } entry;
    struct {
      union {
        std::atomic_flag locked;
        // std::atomic<long> counter;
      };
      hash_val* next;
    } control;
  };

  inline void init_control() {
    control.locked.clear();
    control.next = NULL;
    // control.counter = 0;
  }

  inline void clear() { entry.tag = EMPTY; }

  inline void lock() {
    while (control.locked.test_and_set(std::memory_order_acquire)) {
      // Recommended to do this to reduce branch prediction trash.
      asm volatile("pause\n" : : : "memory");
    }
  }

  inline void unlock() { control.locked.clear(std::memory_order_release); }
};

inline hash_val* create_table(size_t num_rows) {
  hash_val* ret = 0;
  // Aligned cache line.
  assert(posix_memalign((void**)&(ret), 64, num_rows * TBL_WIDTH * sizeof(hash_val)) == 0);

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < num_rows; i++) {
    // First are control.
    ret[i * TBL_WIDTH].init_control();
    // Remaining are slots.
    for (int j = 1; j < TBL_WIDTH; j++) {
      ret[i * TBL_WIDTH + j].entry.tag = EMPTY;
      ret[i * TBL_WIDTH + j].entry.val = 0;
    }
  }
  return ret;
}

class arr_hashtbl : base_hashtbl {
 public:
  void init() override {
    // Try to align it with cache line.
    tbl_ = create_table(1 << TBL_BIT_SIZE);
  }

  bool insert(const key_type& key, value_type& value) override {
    uint32_t hash = myhash(key);
    int bucket = hash * TBL_WIDTH;
    int checked_slot = 0;

    auto& master = tbl_[bucket];
    auto* hcontrol = &tbl_[bucket];
    auto* hentry = hcontrol + 1;
    hash_val* empty_hentry = NULL;

    master.lock();
    while (1) {
      auto tag = hentry->entry.tag;
      // If the key is the same as tag, someone has inserted it.
      if (tag == key) {
        value.v = hentry->entry.val;
        hentry->clear();
        master.unlock();
        return false;
      } else if (tag == EMPTY) {
        // Ortherwise, if the tag is empty, we record the slot.
        // We can't return until we go over all entries.
        empty_hentry = hentry;
      }

      hentry++;
      checked_slot++;
      // If we go over all entry, means no empty slot.
      if (checked_slot == (TBL_WIDTH - 1)) {
        // Moving on to the next.
        // *** SLOWISH ***
        if (hcontrol->control.next == NULL) {
          // This is the end of the table, if we still not found
          // create new table.
          if (empty_hentry == NULL) {
            hcontrol->control.next = create_table(1);
            hcontrol = hcontrol->control.next;
            empty_hentry = hcontrol + 1;
          }
          break;
        } else {
          // Otherwise, moving on.
          hcontrol = hcontrol->control.next;
          hentry = hcontrol + 1;
          checked_slot = 0;
        }
      }
    }
    empty_hentry->entry.tag = key;
    empty_hentry->entry.val = value.v;
    master.unlock();
    return true;
  }

 private:
  hash_val* tbl_;
};

#endif
