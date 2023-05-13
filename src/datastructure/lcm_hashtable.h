#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_

#define LCM_HASHTABLE_EMPTY ((uint64_t)-1)
#define LCM_HASHTABLE_BIT_SIZE \
  16  // size: 1 << 16 (1 control block 3 key-value pairs)
#define LCM_HASHTABLE_WIDTH 4

#ifdef __cplusplus
extern "C" {
#endif
typedef uintptr_t LCM_hashtable_val;
typedef uint64_t LCM_hashtable_key;

typedef struct LCM_hashtable_t {
  union {
    struct {
      LCM_hashtable_key tag;
      LCM_hashtable_val val;
    } entry;
    struct {
      LCIU_spinlock_t lock;
      struct LCM_hashtable_t* next;
    } control;
  };
  // NOTE: This must be aligned to 16, make sure TBL_WDITH is 4,
  // So they will fit in a cache line.
} LCM_hashtable_t;

enum LCM_hashtable_insert_type { CLIENT, SERVER };
void LCM_hashtable_init(LCM_hashtable_t** h);
void LCM_hashtable_fina(LCM_hashtable_t* h);
static inline int LCM_hashtable_insert(LCM_hashtable_t* h,
                                       LCM_hashtable_key key,
                                       LCM_hashtable_val* value,
                                       enum LCM_hashtable_insert_type type);

#ifdef __cplusplus
}
#endif

// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
static const uint32_t Prime = 0x01000193;  //   16777619
static const uint32_t Seed = 0x811C9DC5;   // 2166136261
#define TINY_MASK(x) (((uint32_t)1 << (x)) - 1)
#define FNV1_32_INIT ((uint32_t)2166136261)

static inline uint32_t myhash(const uint64_t k)
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
  return (((hash >> LCM_HASHTABLE_BIT_SIZE) ^ hash) &
          TINY_MASK(LCM_HASHTABLE_BIT_SIZE));
}

static inline LCM_hashtable_t* LCMI_hashtable_create_table(size_t num_rows)
{
  LCM_hashtable_t* ret = 0;
  posix_memalign((void**)&ret, LCI_CACHE_LINE,
                 num_rows * LCM_HASHTABLE_WIDTH * sizeof(LCM_hashtable_t));

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < num_rows; i++) {
    // First are control.
    LCIU_spinlock_init(&ret[i * LCM_HASHTABLE_WIDTH].control.lock);
    ret[i * LCM_HASHTABLE_WIDTH].control.next = NULL;

    // Remaining are slots.
    for (int j = 1; j < LCM_HASHTABLE_WIDTH; j++) {
      ret[i * LCM_HASHTABLE_WIDTH + j].entry.tag = LCM_HASHTABLE_EMPTY;
      ret[i * LCM_HASHTABLE_WIDTH + j].entry.val = 0;
    }
  }
  return ret;
}

static inline void LCMI_hashtable_free_table(LCM_hashtable_t* p)
{
  // TODO: LCIU_spinlock_fina(&ret[i * TBL_WIDTH].control.lock);
  free(p);
}

static inline int LCM_hashtable_insert(LCM_hashtable_t* h,
                                       LCM_hashtable_key key,
                                       LCM_hashtable_val* value,
                                       enum LCM_hashtable_insert_type type)
{
  LCM_hashtable_t* tbl_ = (LCM_hashtable_t*)h;

  const uint32_t hash = myhash(key);
  const int bucket = hash * LCM_HASHTABLE_WIDTH;
  int checked_slot = 0;
  bool found = false;
  int n_empty_slots = 0;

  LCM_hashtable_t* master = &tbl_[bucket];
  LCM_hashtable_t* hcontrol = &tbl_[bucket];
  LCM_hashtable_t* hentry = hcontrol + 1;
  LCM_hashtable_t* empty_hentry = NULL;
  LCM_hashtable_t* pre_hcontrol = NULL;

  LCM_hashtable_key cmp_key = (key << 1) | (1 - type);

  LCIU_acquire_spinlock(&master->control.lock);
  while (1) {
    LCM_hashtable_key tag = hentry->entry.tag;
    // If the key is the same as tag, someone has inserted it.
    // If the type is different, meaning we can't use it.
    if (tag == cmp_key) {
      *value = hentry->entry.val;
      hentry->entry.tag = LCM_HASHTABLE_EMPTY;
      found = true;
      break;
    } else if (tag == LCM_HASHTABLE_EMPTY) {
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

    hentry++;
    checked_slot++;
    // If we go over all entry, means no matched slot.
    if (checked_slot % (LCM_HASHTABLE_WIDTH - 1) == 0) {
      // Check whether we can remove this table
      if (pre_hcontrol != NULL && n_empty_slots == LCM_HASHTABLE_WIDTH - 1) {
        pre_hcontrol->control.next = hcontrol->control.next;
        LCMI_hashtable_free_table(hcontrol);
        hcontrol = pre_hcontrol;
      }
      // Moving on to the next.
      // *** SLOWISH ***
      if (hcontrol->control.next == NULL) {
        // This is the end of the table,
        if (empty_hentry == NULL) {
          // if we still not found an empty entry, create new table.
          hcontrol->control.next = LCMI_hashtable_create_table(1);
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
  LCII_PCOUNTERS_WRAPPER(
      LCII_pcounters[LCIU_get_thread_id()].hashtable_insert_num++);
  LCII_PCOUNTERS_WRAPPER(
      LCII_pcounters[LCIU_get_thread_id()].hashtable_walk_steps_total +=
      checked_slot);
  LCII_PCOUNTERS_WRAPPER(LCIU_MAX_ASSIGN(
      LCII_pcounters[LCIU_get_thread_id()].hashtable_walk_steps_max,
      checked_slot));
  if (found) {
    LCIU_release_spinlock(&master->control.lock);
    LCM_DBG_Log(LCM_LOG_DEBUG, "hashtable", "insert (%lx, %p, %d), return 0\n",
                key, value, type);
  } else {
    empty_hentry->entry.tag = (key << 1) | type;
    empty_hentry->entry.val = *value;
    LCIU_release_spinlock(&master->control.lock);
    LCM_DBG_Log(LCM_LOG_DEBUG, "hashtable", "insert (%lx, %p, %d), return 1\n",
                key, value, type);
  }
  return !found;
}

#endif
