#include "lc/hashtable.h"

#include "lc/lock.h"
#include "lc/macro.h"

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


