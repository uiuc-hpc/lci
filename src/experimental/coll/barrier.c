#include "lci.h"
#include "runtime/lcii.h"
#include "experimental/coll/coll.h"

LCI_error_t LCIX_barrier(LCI_endpoint_t ep, LCI_tag_t tag,
                         LCI_comp_t completion, void* user_context,
                         LCIX_collective_t* collp)
{
  size_t sched_ops;
  int rank, size;
  LCI_mbuffer_t empty = {.address = NULL, .length = 0};

  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;

  /* Special case for size == 1 */
  if (size <= 1) {
    *collp = NULL;
    LCIXC_mcoll_complete(ep, empty, tag, completion, user_context);
    return LCI_OK;
  }

  /* We have at most 2 * ilog2(size) scheduled operations */
  sched_ops = 2 * ilog2(size);

  LCIX_collective_t coll = LCIU_malloc(sizeof(struct LCIX_collective_s));
  LCIXC_mcoll_init(coll, ep, tag, NULL, completion, user_context, empty,
                   sched_ops);

  /* Exchange data with rank-2^k and rank+2^k */
  for (int distance = 1; distance < size; distance <<= 1) {
    int from = (rank + size - distance) % size;
    int to = (rank + distance) % size;

    /* Send message to lower ranked node */
    LCIXC_mcoll_sched(coll, LCIXC_COLL_SEND, to, empty, empty);
    LCIXC_mcoll_sched(coll, LCIXC_COLL_RECV, from, empty, empty);
  }

  *collp = coll;
  return LCI_OK;
}
