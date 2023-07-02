#include "runtime/lcii.h"

#define LCII_MATCHTABLE_HASH_EMPTY ((uint64_t)-1)
#define LCII_MATCHTABLE_HASH_BIT_SIZE \
  16  // size: 1 << 16 (1 control block 3 key-value pairs)
#define LCII_MATCHTABLE_HASH_WIDTH 4

typedef struct LCII_matchtable_hash_t {
  union {
    struct {
      uint64_t tag;
      uint64_t val;
    } entry;
    struct {
      LCIU_spinlock_t lock;
      struct LCII_matchtable_hash_t* next;
    } control;
  };
  // NOTE: This must be aligned to 16, make sure TBL_WDITH is 4,
  // So they will fit in a cache line.
} LCII_matchtable_hash_t;

static inline uint32_t LCII_matchtable_hash_hash(const uint64_t k)
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
  return (((hash >> LCII_MATCHTABLE_HASH_BIT_SIZE) ^ hash) &
          ((uint32_t)1 << LCII_MATCHTABLE_HASH_BIT_SIZE) - 1);
}

static inline LCII_matchtable_hash_t* LCII_matchtable_hash_create_internal(
    size_t num_rows)
{
  LCII_matchtable_hash_t* ret = 0;
  ret = LCIU_malloc(num_rows * LCII_MATCHTABLE_HASH_WIDTH *
                    sizeof(LCII_matchtable_hash_t));

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < num_rows; i++) {
    // First are control.
    LCIU_spinlock_init(&ret[i * LCII_MATCHTABLE_HASH_WIDTH].control.lock);
    ret[i * LCII_MATCHTABLE_HASH_WIDTH].control.next = NULL;

    // Remaining are slots.
    for (int j = 1; j < LCII_MATCHTABLE_HASH_WIDTH; j++) {
      ret[i * LCII_MATCHTABLE_HASH_WIDTH + j].entry.tag =
          LCII_MATCHTABLE_HASH_EMPTY;
      ret[i * LCII_MATCHTABLE_HASH_WIDTH + j].entry.val = 0;
    }
  }
  return ret;
}

void LCII_matchtable_hash_free_internal(LCI_matchtable_t* mt_p, size_t num_rows)
{
  LCII_matchtable_hash_t* mt = *(LCII_matchtable_hash_t**)mt_p;
  for (size_t i = 0; i < num_rows; i++) {
    LCIU_spinlock_fina(&mt[i * LCII_MATCHTABLE_HASH_WIDTH].control.lock);
    // TODO: free all the lock in link list
  }
  LCIU_free(mt);
  *mt_p = NULL;
}

LCI_error_t LCII_matchtable_hash_insert(LCI_matchtable_t mt, uint64_t key,
                                        uint64_t* value,
                                        enum LCII_matchtable_insert_type type)
{
  LCII_matchtable_hash_t* tbl_ = (LCII_matchtable_hash_t*)mt;

  const uint32_t hash = LCII_matchtable_hash_hash(key);
  const int bucket = hash * LCII_MATCHTABLE_HASH_WIDTH;
  int checked_slot = 0;
  LCI_error_t ret = LCI_ERR_RETRY;
  bool cannot_found = false;
  int n_empty_slots = 0;

  LCII_matchtable_hash_t* master = &tbl_[bucket];
  LCII_matchtable_hash_t* hcontrol = &tbl_[bucket];
  LCII_matchtable_hash_t* hentry = hcontrol + 1;
  LCII_matchtable_hash_t* empty_hentry = NULL;
  LCII_matchtable_hash_t* pre_hcontrol = NULL;

  uint64_t my_key = (key << 1) | type;
  uint64_t cmp_key = (key << 1) | (1 - type);

  LCIU_acquire_spinlock(&master->control.lock);
  while (1) {
    uint64_t tag = hentry->entry.tag;
    // If the key is the same as tag, someone has inserted it.
    // If the type is different, meaning we can't use it.
    if (tag == cmp_key) {
      *value = hentry->entry.val;
      hentry->entry.tag = LCII_MATCHTABLE_HASH_EMPTY;
      ret = LCI_OK;
      break;
    } else if (tag == my_key) {
      // We will not find what we want, just find an empty slot.
      cannot_found = true;
    } else if (tag == LCII_MATCHTABLE_HASH_EMPTY) {
      // Otherwise, if the tag is empty, we record the slot.
      // We can't return until we go over all entries.
      if (empty_hentry == NULL)
        empty_hentry = hentry;
      else
        ++n_empty_slots;
    } else {
      // If we are still seeing some non-empty,
      // push that empty entry even further.
      // FIXME: Why pushing the empty entry further?
      //      empty_hentry = NULL;
    }

    if (cannot_found && empty_hentry) break;

    hentry++;
    checked_slot++;
    // If we go over all entry, means no matched slot.
    if (checked_slot % (LCII_MATCHTABLE_HASH_WIDTH - 1) == 0) {
      // Check whether we can remove this table
      if (pre_hcontrol != NULL &&
          n_empty_slots == LCII_MATCHTABLE_HASH_WIDTH - 1) {
        pre_hcontrol->control.next = hcontrol->control.next;
        LCII_matchtable_hash_free_internal((LCI_matchtable_t*)&hcontrol, 1);
        hcontrol = pre_hcontrol;
      }
      // Moving on to the next.
      // *** SLOWISH ***
      if (hcontrol->control.next == NULL) {
        // This is the end of the table,
        if (empty_hentry == NULL) {
          // if we still not found an empty entry, create new table.
          hcontrol->control.next = LCII_matchtable_hash_create_internal(1);
          hcontrol = hcontrol->control.next;
          empty_hentry = hcontrol + 1;
        }
        break;
      } else {
        // Otherwise, moving on.
        n_empty_slots = 0;
        pre_hcontrol = hcontrol;
        hcontrol = hcontrol->control.next;
        hentry = hcontrol + 1;
      }
    }
  }
  LCI_DBG_Assert(!(ret == LCI_OK && cannot_found),
                 "Unexpected result! Something is wrong!\n");
  if (ret == LCI_ERR_RETRY) {
    empty_hentry->entry.tag = (key << 1) | type;
    empty_hentry->entry.val = *value;
  }
  LCI_DBG_Log(LCI_LOG_TRACE, "matchtable", "insert (%lx, %p, %d), return 1\n",
              key, value, type);
  LCIU_release_spinlock(&master->control.lock);
  return ret;
}

void LCII_matchtable_hash_create(LCI_matchtable_t* mt_p)
{
  int num_rows = 1 << LCII_MATCHTABLE_HASH_BIT_SIZE;
  LCII_matchtable_hash_t* mt = LCII_matchtable_hash_create_internal(num_rows);
  *mt_p = (LCI_matchtable_t)mt;
}

void LCII_matchtable_hash_free(LCI_matchtable_t* mt_p)
{
  int num_rows = 1 << LCII_MATCHTABLE_HASH_BIT_SIZE;
  LCII_matchtable_hash_free_internal(mt_p, num_rows);
}

void LCII_matchtable_hash_setup_ops(struct LCII_matchtable_ops_t* ops)
{
  ops->create = LCII_matchtable_hash_create;
  ops->free = LCII_matchtable_hash_free;
  ops->insert = LCII_matchtable_hash_insert;
}
