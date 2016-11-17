#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_

// #include "config.h"
#include "hashtbl.h"
#include <atomic>
#include <assert.h>
#include <mutex>
#include <stdexcept>

#include "lock.h"
#include "macro.h"

struct arr_hash_val;

inline void arr_hash_init(mv_hash** h);
inline void arr_hash_insert(mv_hash*, mv_key*, mv_value*);
inline static uint32_t myhash(const uint64_t k);
inline arr_hash_val* create_table(size_t num_rows);

static const uint64_t EMPTY = (uint64_t)-1;
static const int TBL_BIT_SIZE = 8;
static const int TBL_WIDTH = 4;

struct arr_hash_val {
  union {
    struct {
      uint64_t tag;
      uintptr_t val;
    } entry;
    struct {
      volatile int lock;
      arr_hash_val* next;
    } control;
  };
} __attribute__((aligned(64)));

inline void arr_hash_init(mv_hash** h) {
  arr_hash_val** hv = (arr_hash_val**) h;
  *hv = create_table(1 << TBL_BIT_SIZE);
}
#undef hash_init
#define hash_init arr_hash_init

inline bool arr_hash_insert(mv_hash* h, const mv_key& key, mv_value& value) {
  arr_hash_val* tbl_ = (arr_hash_val*) h;

  uint32_t hash = myhash(key);
  int bucket = hash * TBL_WIDTH;
  int checked_slot = 0;

  arr_hash_val* master = &tbl_[bucket];
  arr_hash_val* hcontrol = &tbl_[bucket];
  arr_hash_val* hentry = hcontrol + 1;
  arr_hash_val* empty_hentry = NULL;

  mv_spin_lock(&master->control.lock);
  while (1) {
    auto tag = hentry->entry.tag;
    // If the key is the same as tag, someone has inserted it.
    if (tag == key) {
      value = hentry->entry.val;
      hentry->entry.tag = EMPTY;
      mv_spin_unlock(&master->control.lock);
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
  empty_hentry->entry.val = value;
  mv_spin_unlock(&master->control.lock);
  return true;
}
#undef hash_insert
#define hash_insert arr_hash_insert

// static_assert((1 << TBL_BIT_SIZE) >= 4 * MAX_CONCURRENCY,
//              "Hash table is not large enough");

// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
static const uint32_t Prime = 0x01000193;  //   16777619
static const uint32_t Seed = 0x811C9DC5;   // 2166136261
#define TINY_MASK(x) (((u_int32_t)1 << (x)) - 1)
#define FNV1_32_INIT ((u_int32_t)2166136261)

static inline uint32_t myhash(const uint64_t k) {
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

inline arr_hash_val* create_table(size_t num_rows) {
  arr_hash_val* ret = 0;
  // Aligned cache line.
  assert(posix_memalign((void**)&(ret), 64,
        num_rows * TBL_WIDTH * sizeof(arr_hash_val)) == 0);

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < num_rows; i++) {
    // First are control.
    ret[i * TBL_WIDTH].control.lock = MV_SPIN_UNLOCKED;
    ret[i * TBL_WIDTH].control.next = NULL;

    // Remaining are slots.
    for (int j = 1; j < TBL_WIDTH; j++) {
      ret[i * TBL_WIDTH + j].entry.tag = EMPTY;
      ret[i * TBL_WIDTH + j].entry.val = 0;
    }
  }
  return ret;
}

#endif
