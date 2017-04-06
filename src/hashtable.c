#include "hashtable.h"

#include "lc/lock.h"
#include "lc/macro.h"

LC_INLINE hash_val* create_table(size_t num_rows);
LC_INLINE uint32_t myhash(const uint64_t k);

void lc_hash_create(lc_hash** h)
{
  struct hash_val** hv = (struct hash_val**)h;
  *hv = create_table(1 << TBL_BIT_SIZE);
}

void lc_hash_destroy(lc_hash* h)
{
  free(h);
}

int lc_hash_insert(lc_hash* h, lc_key key, lc_value* value)
{
  struct hash_val* tbl_ = (struct hash_val*)h;

  const uint32_t hash = myhash(key);
  const int bucket = hash * TBL_WIDTH;
  int checked_slot = 0;

  hash_val* master = &tbl_[bucket];
  hash_val* hcontrol = &tbl_[bucket];
  hash_val* hentry = hcontrol + 1;
  hash_val* empty_hentry = NULL;

  lc_spin_lock(&master->control.lock);
  while (1) {
    lc_key tag = hentry->entry.tag;
    // If the key is the same as tag, someone has inserted it.
    if (tag == key) {
      *value = hentry->entry.val;
      hentry->entry.tag = EMPTY;
      lc_spin_unlock(&master->control.lock);
      return 0;
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
  empty_hentry->entry.val = *value;
  lc_spin_unlock(&master->control.lock);
  return 1;
}

// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
static const uint32_t Prime = 0x01000193; //   16777619
static const uint32_t Seed = 0x811C9DC5;  // 2166136261
#define TINY_MASK(x) (((uint32_t)1 << (x)) - 1)
#define FNV1_32_INIT ((uint32_t)2166136261)

LC_INLINE uint32_t myhash(const uint64_t k)
{
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

LC_INLINE hash_val* create_table(size_t num_rows)
{
  hash_val* ret = NULL;
  posix_memalign((void**)&(ret), 64,
                 num_rows * TBL_WIDTH * sizeof(hash_val));

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < num_rows; i++) {
    // First are control.
    ret[i * TBL_WIDTH].control.lock = LC_SPIN_UNLOCKED;
    ret[i * TBL_WIDTH].control.next = NULL;

    // Remaining are slots.
    for (int j = 1; j < TBL_WIDTH; j++) {
      ret[i * TBL_WIDTH + j].entry.tag = EMPTY;
      ret[i * TBL_WIDTH + j].entry.val = 0;
    }
  }
  return ret;
}
