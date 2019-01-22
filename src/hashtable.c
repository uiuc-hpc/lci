#include "lc/hashtable.h"

#include "lc/lock.h"
#include "lc/macro.h"

void lc_hash_create(struct lc_hash** hv)
{
  *hv = create_table(1 << TBL_BIT_SIZE);
}

void lc_hash_destroy(struct lc_hash* h)
{
  // FIXME: LEAK, need to destroy extended table.
  free(h);
}


