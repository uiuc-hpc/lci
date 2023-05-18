#include "runtime/lcii.h"

bool initialized = false;
struct LCII_matchtable_ops_t LCII_matchtable_ops;

void LCII_matchtable_hash_setup_ops(struct LCII_matchtable_ops_t* ops);
void LCII_matchtable_queue_setup_ops(struct LCII_matchtable_ops_t* ops);
void LCII_matchtable_hashqueue_setup_ops(struct LCII_matchtable_ops_t* ops);

void initialize_ops()
{
  char* p = getenv("LCI_MT_BACKEND");
  if (p == NULL) {
    p = LCI_MT_BACKEND_DEFAULT;
  }
  if (strcmp(p, "hashqueue") == 0 || strcmp(p, "HASHQUEUE") == 0) {
    LCM_Log(LCM_LOG_INFO, "mt",
            "Use `hash queue` as the matching table backend.\n");
    LCII_matchtable_hashqueue_setup_ops(&LCII_matchtable_ops);
  } else if (strcmp(p, "hash") == 0 || strcmp(p, "HASH") == 0) {
    LCM_Log(LCM_LOG_INFO, "mt", "Use `hash` as the matching table backend.\n");
    LCII_matchtable_hash_setup_ops(&LCII_matchtable_ops);
  } else if (strcmp(p, "queue") == 0 || strcmp(p, "QUEUE") == 0) {
    LCM_Log(LCM_LOG_INFO, "mt", "Use `queue` as the matching table backend.\n");
    LCII_matchtable_queue_setup_ops(&LCII_matchtable_ops);
  } else
    LCM_Log_default(LCM_LOG_WARN,
                    "unknown env LCI_MT_BACKEND (%s against "
                    "hash|queue|hashqueue). use the default hash.\n",
                    p);
  initialized = true;
}

void LCII_matchtable_create(LCI_matchtable_t* mt_p)
{
  if (!initialized) {
    initialize_ops();
  }
  LCII_matchtable_ops.create(mt_p);
}

void LCII_matchtable_free(LCI_matchtable_t* mt_p)
{
  LCM_Assert(initialized, "\n");
  LCII_matchtable_ops.free(mt_p);
}

LCI_error_t LCII_matchtable_insert(LCI_matchtable_t mt, uint64_t key,
                                   uint64_t* value,
                                   enum LCII_matchtable_insert_type type)
{
  LCM_Assert(initialized, "\n");
  return LCII_matchtable_ops.insert(mt, key, value, type);
}