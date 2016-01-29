#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_

#include <mutex>
#include <stdexcept>

static const uint64_t EMPTY = (uint64_t)-1;
static const int TBL_BIT_SIZE = 7;
static const int TBL_WIDTH = 4;

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
      std::atomic_flag locked;
      hash_val* next;
    } control;
  };

  inline void init_control() {
    control.locked.clear();
    control.next = NULL;
  }

  inline void clear() {
    entry.tag = EMPTY;
    entry.val = 0;
  }

  inline void set(uint64_t tag, void* val) {
    entry.tag = tag;
    entry.val = val;
  }

  inline void lock() {
    while (control.locked.test_and_set(std::memory_order_acquire)) {
      ;
    }
  }

  inline void unlock() { control.locked.clear(std::memory_order_release); }
};

inline hash_val* create_table(size_t num_rows) {
  hash_val* ret = 0;
  // Aligned cache line.
  posix_memalign((void**)&(ret), 64, num_rows * TBL_WIDTH * sizeof(hash_val));

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < num_rows; i++) {
    // First are control.
    ret[i * TBL_WIDTH].init_control();
    // Remaining are slots.
    for (int j = 1; j < TBL_WIDTH; j++) {
      ret[i * TBL_WIDTH + j].clear();
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

  pair<value_type, hint_type> insert(const key_type& key,
                                     const value_type& value) override {
    value_type ret = value;
    uint32_t hash = myhash(key);
    int bucket = hash * TBL_WIDTH;
    int checked_slot = 0;
    int tried = 0;

    auto& master = tbl_[bucket];
    auto* hcontrol = &tbl_[bucket];
    auto* hentry = hcontrol + 1;
    hash_val* empty_hentry = NULL;

    master.lock();
    while (tried < 4) {
      auto tag = hentry->entry.tag;
      // If the key is the same as tag, someone has inserted it.
      if (tag == key) {
        ret.v = hentry->entry.val;
        master.unlock();
        return make_pair(ret, (uintptr_t)hentry);
      } else if (tag == EMPTY) {
        // Ortherwise, if the tag is empty, we record the slot.
        // We can't return until we go over all entries.
        empty_hentry = hentry;
      }

      hentry++;
      checked_slot += 1;
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
          empty_hentry->set(key, value.v);
          master.unlock();
          return make_pair(ret, (uintptr_t)empty_hentry);
        } else {
          // Otherwise, moving on.
          hcontrol = hcontrol->control.next;
          hentry = hcontrol + 1;
          tried += 1;
          checked_slot = 0;
        }
      }
    }
    master.unlock();
    throw new hash_table_error("Too many extra slot in the hash table");
  }

  void erase(const key_type& key, hint_type hint) override {
    uint32_t hash = myhash(key);

    int bucket = hash * TBL_WIDTH;
    auto* hentry = (hash_val*)hint;
    tbl_[bucket].lock();
    hentry->clear();
    tbl_[bucket].unlock();
  }

 private:
  hash_val* tbl_;
};

#endif
