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

#include <stdio.h>
#include <stdint.h>
LC_EXPORT uint32_t lc_hash_compute(const uint64_t k) { return myhash(k); }
/* only call during debug, all threads stopped - not thread safe */
LC_EXPORT void lc_hash_dump(lc_hash* h, size_t num_rows)
{
    if (h == NULL)
        return;

    if (num_rows == 0)
        num_rows = (1 << TBL_BIT_SIZE);

    /* h is array divided into `num_rows' buckets of `TBL_WIDTH' length
     * first entry of bucket is control block with lock and extension pointer
     * remainder are actual entries
     * process actual entries before checking if bucket is extended
     * only print non-empty entries */
    for (size_t i = 0; i < num_rows; i++) {
        size_t bucket = i * TBL_WIDTH;
        for (size_t j = bucket+1; j < bucket+TBL_WIDTH; j++) {
            if (h[j].entry.tag != EMPTY) {
                int server = (h[j].entry.tag & SERVER);
                lc_key key = (h[j].entry.tag >> 1);
                int tag = (key & UINT32_MAX);
                int rank = (key >> 32);
                void *pointer = (void *)(h[j].entry.val);
                if (server) {
                    fprintf(stderr, "bucket[%zu] SERVER rank=%d tag=%d lc_packet*=%p\n",
                            bucket, rank, tag, pointer);
                } else {
                    fprintf(stderr, "bucket[%zu] CLIENT rank=%d tag=%d lc_req*=%p\n",
                            bucket, rank, tag, pointer);
                }
            }
        }
        lc_hash_dump(h[bucket].control.next, 1);
    }
}
