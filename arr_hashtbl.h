#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_
#include <mutex>

class spin_lock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
    volatile int insert;
    volatile int empty;
 public:
    spin_lock() {
        insert = 0;
        empty = 0;
    }

    inline void enter_insert() {
        retry:
        while (empty > 0) { }
        lock();
            if (empty == 0) insert ++;
            else {
                unlock();
                goto retry;
            }
        unlock();
    }

    inline void exit_insert() {
        lock();
        insert --;
        unlock();
    }

    inline void enter_empty() {
        retry:
        while (insert > 0)  { ; }
        lock();
            if (insert == 0) empty ++;
            else {
                unlock();
                goto retry;
            }
        unlock();
    }

    inline void exit_empty() {
        lock();
        empty --;
        unlock();
    }

    inline void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }
    inline void unlock() {
        locked.clear(std::memory_order_release);
    }
};

static const uint64_t EMPTY = (uint64_t) -1;
static const int TBL_BIT_SIZE = 7;
static const int TBL_WIDTH = 16;

spin_lock sl[1 << TBL_BIT_SIZE];

// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
static const uint32_t Prime = 0x01000193; //   16777619
static const uint32_t Seed  = 0x811C9DC5; // 2166136261
#define TINY_MASK(x) (((u_int32_t)1<<(x))-1)
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
    return (((hash>>TBL_BIT_SIZE) ^ hash) & TINY_MASK(TBL_BIT_SIZE));
}

struct hash_val {
    union {
      struct {
        uint64_t tag;
        void* val;
      } entry;
      std::atomic_flag locked;
    };

    inline void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }

    inline void unlock() {
        locked.clear(std::memory_order_release);
    }
};

class arr_hashtbl : base_hashtbl {
 public:
  void init() override {
    // Try to align it with cache line.
    posix_memalign((void**) &(tbl_), 64, (1<<TBL_BIT_SIZE) * TBL_WIDTH * sizeof(hash_val));

    // Initialize all with EMPTY and clear lock.
    for (int i = 0; i < (1<<TBL_BIT_SIZE); i++) {
        for (int j = 0; j < TBL_WIDTH - 1; j++) {
            tbl_[(i * TBL_WIDTH) + j + 1].entry = {EMPTY, 0};
        }
        tbl_[i * TBL_WIDTH].locked.clear();
    }
  }

  pair<value_type, hint_type> insert(const key_type& key, const value_type& value) override {
    value_type ret = value;
    uint32_t hash = myhash(key);
    int bucket = hash * TBL_WIDTH;
    int slot = 0;
    int empty_slot = -1;

    tbl_[bucket].lock();
    while (1) {
        register uint64_t tag = tbl_[slot + bucket + 1].entry.tag;
        // If the key is the same as tag, someone has inserted it.
        if (tag == key) {
            ret.v = tbl_[slot + bucket + 1].entry.val;
            tbl_[bucket].unlock();
            return make_pair(ret, slot);
        } else if (tag == EMPTY && empty_slot == -1) {
            // Ortherwise, if the tag is empty, we record the slot.
            empty_slot = slot;
        }
        slot = (slot + 1) % (TBL_WIDTH - 1);
        // If we go over all entry once, and we found the empty slot.
        if (slot == 0 && empty_slot != -1) {
            // Insert the value in the empty one.
            tbl_[empty_slot + bucket + 1].entry = {key, value.v};
            break;
        }
        // Retry otherwise.
    }
    tbl_[bucket].unlock();
    return make_pair(ret, empty_slot);
  }

  void erase(const key_type& key, hint_type hint) override {
    uint32_t hash = myhash(key);
    int bucket = hash * TBL_WIDTH;
    tbl_[bucket].lock();
    tbl_[bucket + hint + 1].entry = {EMPTY, 0};
    tbl_[bucket].unlock();
  }

 private:
  hash_val* tbl_;
};

#endif
