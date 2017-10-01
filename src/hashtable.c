#include "lc/hashtable.h"

#include "lc/lock.h"
#include "lc/macro.h"

LC_INLINE struct hash_val* create_table(size_t num_rows)
{
  struct hash_val* ret = 0;
  posix_memalign((void**) &ret, 64,
      num_rows * TBL_WIDTH * sizeof(struct hash_val));

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

void lc_hash_create(lc_hash** h)
{
  struct hash_val** hv = (struct hash_val**)h;
  *hv = create_table(1 << TBL_BIT_SIZE);
}

void lc_hash_destroy(lc_hash* h)
{
  // FIXME: LEAK, need to destroy extended table.
  free(h);
}

int lc_hash_insert(lc_hash* h, lc_key key, lc_value* value,
    enum insert_type type)
{
  struct hash_val* tbl_ = (struct hash_val*)h;

  const uint32_t hash = myhash(key);
  const int bucket = hash * TBL_WIDTH;
  int checked_slot = 0;

  struct hash_val* master = &tbl_[bucket];
  struct hash_val* hcontrol = &tbl_[bucket];
  struct hash_val* hentry = hcontrol + 1;
  struct hash_val* empty_hentry = NULL;

  lc_key cmp_key = (key << 1) | (1-type);

  lc_spin_lock(&master->control.lock);
  while (1) {
    lc_key tag = hentry->entry.tag;
    // If the key is the same as tag, someone has inserted it.
    // If the type is different, meaning we can't use it.
    if (tag == cmp_key) {
      *value = hentry->entry.val;
      hentry->entry.tag = EMPTY;
      lc_spin_unlock(&master->control.lock);
      return 0;
    } else if (tag == EMPTY) {
      // Ortherwise, if the tag is empty, we record the slot.
      // We can't return until we go over all entries.
      if (empty_hentry == NULL)
        empty_hentry = hentry;
    } else {
      // If we are still seeing some non-empty,
      // push that empty entry even further.
      empty_hentry = NULL;
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
  empty_hentry->entry.tag = (key << 1) | type;
  empty_hentry->entry.val = *value;
  lc_spin_unlock(&master->control.lock);
  return 1;
}
