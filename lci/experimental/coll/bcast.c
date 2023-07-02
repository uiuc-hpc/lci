#include "lci.h"
#include "runtime/lcii.h"
#include "experimental/coll/coll.h"

LCI_error_t LCIX_bcastm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int root,
                        LCI_tag_t tag, LCI_comp_t completion,
                        void* user_context, LCIX_collective_t* collp)
{
  size_t sched_ops;
  int rank, size;
  LCIX_collective_t coll;

  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;

  /* Special case for size == 1 */
  if (size <= 1) {
    *collp = NULL;
    LCIXC_mcoll_complete(ep, buffer, tag, completion, user_context);
    return LCI_OK;
  }

  /* Linear broadcast, only the root does anything really */
  sched_ops = (rank == root) ? size - 1 : 1;

  coll = LCIU_malloc(sizeof(struct LCIX_collective_s));
  LCIXC_mcoll_init(coll, ep, tag, NULL, completion, user_context, buffer,
                   sched_ops);

  if (root == rank) {
    /* Root sends data to everyone */
    for (int i = 0; i < size; i++) {
      if (i == rank) continue;
      LCIXC_mcoll_sched(coll, LCIXC_COLL_SEND, i, buffer, buffer);
    }
  } else {
    LCIXC_mcoll_sched(coll, LCIXC_COLL_RECV, root, buffer, buffer);
  }

  *collp = coll;
  return LCI_OK;
}

LCI_error_t LCIX_bcastl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, int root,
                        LCI_tag_t tag, LCI_comp_t completion,
                        void* user_context, LCIX_collective_t* collp)
{
  size_t sched_ops;
  int rank, size;
  LCIX_collective_t coll;

  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;

  /* Special case for size == 1 */
  if (size <= 1) {
    *collp = NULL;
    LCIXC_lcoll_complete(ep, buffer, tag, completion, user_context);
    return LCI_OK;
  }

  /* Linear broadcast, only the root does anything really */
  sched_ops = (rank == root) ? size - 1 : 1;

  coll = LCIU_malloc(sizeof(struct LCIX_collective_s));
  LCIXC_lcoll_init(coll, ep, tag, NULL, completion, user_context, buffer,
                   sched_ops);

  if (root == rank) {
    /* Root sends data to everyone */
    for (int i = 0; i < size; i++) {
      if (i == rank) continue;
      LCIXC_lcoll_sched(coll, LCIXC_COLL_SEND, i, buffer, buffer);
    }
  } else {
    LCIXC_lcoll_sched(coll, LCIXC_COLL_RECV, root, buffer, buffer);
  }

  *collp = coll;
  return LCI_OK;
}
