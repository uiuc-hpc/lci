#include "lci.h"
#include "runtime/lcii.h"
#include "experimental/coll/coll.h"

LCI_error_t LCIX_allreducem(LCI_endpoint_t ep, LCI_mbuffer_t accumulate,
                            LCI_op_t op, LCI_tag_t tag, LCI_comp_t completion,
                            void* user_context, LCIX_collective_t* collp)
{
  size_t sched_ops;
  int rank, size, size_log2, adjsize, newrank, extra_ranks;
  LCIX_collective_t coll;
  LCI_mbuffer_t receive;

  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;

  /* Special case for size == 1 */
  if (size <= 1) {
    *collp = NULL;
    LCIXC_mcoll_complete(ep, accumulate, tag, completion, user_context);
    return LCI_OK;
  }

  /* Allocate receive buffer */
  if (LCI_OK != LCI_mbuffer_alloc(ep->device, &receive)) {
    return LCI_ERR_RETRY;
  }
  /* Set receive buffer length */
  receive.length = accumulate.length;

  /* Determine nearest power of two less than or equal to size */
  size_log2 = ilog2(size);
  adjsize = 1 << size_log2;

  /* All ranks have at most 2 * size_log2 + 3 scheduled ops */
  sched_ops = 2 * size_log2 + 3;

  coll = LCIU_malloc(sizeof(struct LCIX_collective_s));
  LCIXC_mcoll_init(coll, ep, tag, op, completion, user_context, accumulate,
                   sched_ops);

  /* Handle non-power-of-two case:
   * - Even ranks less than 2 * extra_ranks send their data to (rank + 1)
   *   and sets new rank to -1.
   * - Odd ranks less than 2 * extra_ranks receive data from (rank - 1),
   *   apply appropriate operation, and set new rank to rank/2
   * - Everyone else sets rank to rank - extra_ranks
   * */
  extra_ranks = size - adjsize;
  if (rank < (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      LCIXC_mcoll_sched(coll, LCIXC_COLL_SEND, rank + 1, accumulate,
                        accumulate);
      newrank = -1;
    } else {
      LCIXC_mcoll_sched(coll, LCIXC_COLL_RECV, rank - 1, receive, receive);
      /* accumulate = accumulate (op) receive */
      LCIXC_mcoll_sched(coll, LCIXC_COLL_OP, 0, receive, accumulate);
      newrank = rank >> 1;
    }
  } else {
    newrank = rank - extra_ranks;
  }

  /* Communication/Computation loop
   * - Exchange message with remote node.
   * - Perform appropriate operation taking in account order of operations
   * */
  for (int distance = 0x1; distance < adjsize; distance <<= 1) {
    if (newrank < 0) break;
    /* Determine remote node */
    int newremote = newrank ^ distance;
    int remote = (newremote < extra_ranks) ? (newremote * 2 + 1)
                                           : (newremote + extra_ranks);

    /* Exchange the data */
    LCIXC_mcoll_sched(coll, LCIXC_COLL_SENDRECV, remote, accumulate, receive);
    /* accumulate = accumulate (op) receive */
    LCIXC_mcoll_sched(coll, LCIXC_COLL_OP, 0, receive, accumulate);
  }

  /* Handle non-power-of-two case:
   * - Even ranks less than 2 * extra_ranks receive result from (rank + 1)
   * - Odd ranks less than 2 * extra_ranks send result to (rank - 1)
   * */
  if (rank < (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      LCIXC_mcoll_sched(coll, LCIXC_COLL_RECV, rank + 1, receive, receive);
    } else {
      LCIXC_mcoll_sched(coll, LCIXC_COLL_SEND, rank - 1, accumulate,
                        accumulate);
    }
  }

  /* Free allocated receive buffer */
  LCIXC_mcoll_sched(coll, LCIXC_COLL_FREE, 0, receive, receive);

  *collp = coll;
  return LCI_OK;
}

LCI_error_t LCIX_allreducel(LCI_endpoint_t ep, LCI_lbuffer_t accumulate,
                            LCI_op_t op, LCI_tag_t tag, LCI_comp_t completion,
                            void* user_context, LCIX_collective_t* collp)
{
  size_t sched_ops;
  int rank, size, size_log2, adjsize, newrank, extra_ranks;
  LCIX_collective_t coll;
  LCI_lbuffer_t receive;

  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;

  /* Special case for size == 1 */
  if (size <= 1) {
    *collp = NULL;
    LCIXC_lcoll_complete(ep, accumulate, tag, completion, user_context);
    return LCI_OK;
  }

  /* Allocate receive buffer */
  if (LCI_OK != LCI_lbuffer_alloc(ep->device, accumulate.length, &receive)) {
    return LCI_ERR_RETRY;
  }

  /* Determine nearest power of two less than or equal to size */
  size_log2 = ilog2(size);
  adjsize = 1 << size_log2;

  /* All ranks have at most 2 * size_log2 + 3 scheduled ops */
  sched_ops = 2 * size_log2 + 3;

  coll = LCIU_malloc(sizeof(struct LCIX_collective_s));
  LCIXC_lcoll_init(coll, ep, tag, op, completion, user_context, accumulate,
                   sched_ops);

  /* Handle non-power-of-two case:
   * - Even ranks less than 2 * extra_ranks send their data to (rank + 1)
   *   and sets new rank to -1.
   * - Odd ranks less than 2 * extra_ranks receive data from (rank - 1),
   *   apply appropriate operation, and set new rank to rank/2
   * - Everyone else sets rank to rank - extra_ranks
   * */
  extra_ranks = size - adjsize;
  if (rank < (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      LCIXC_lcoll_sched(coll, LCIXC_COLL_SEND, rank + 1, accumulate,
                        accumulate);
      newrank = -1;
    } else {
      LCIXC_lcoll_sched(coll, LCIXC_COLL_RECV, rank - 1, receive, receive);
      /* accumulate = accumulate (op) receive */
      LCIXC_lcoll_sched(coll, LCIXC_COLL_OP, 0, receive, accumulate);
      newrank = rank >> 1;
    }
  } else {
    newrank = rank - extra_ranks;
  }

  /* Communication/Computation loop
   * - Exchange message with remote node.
   * - Perform appropriate operation taking in account order of operations
   * */
  for (int distance = 0x1; distance < adjsize; distance <<= 1) {
    if (newrank < 0) break;
    /* Determine remote node */
    int newremote = newrank ^ distance;
    int remote = (newremote < extra_ranks) ? (newremote * 2 + 1)
                                           : (newremote + extra_ranks);

    /* Exchange the data */
    LCIXC_lcoll_sched(coll, LCIXC_COLL_SENDRECV, remote, accumulate, receive);
    /* accumulate = accumulate (op) receive */
    LCIXC_lcoll_sched(coll, LCIXC_COLL_OP, 0, receive, accumulate);
  }

  /* Handle non-power-of-two case:
   * - Even ranks less than 2 * extra_ranks receive result from (rank + 1)
   * - Odd ranks less than 2 * extra_ranks send result to (rank - 1)
   * */
  if (rank < (2 * extra_ranks)) {
    if (0 == (rank % 2)) {
      LCIXC_lcoll_sched(coll, LCIXC_COLL_RECV, rank + 1, receive, receive);
    } else {
      LCIXC_lcoll_sched(coll, LCIXC_COLL_SEND, rank - 1, accumulate,
                        accumulate);
    }
  }

  /* Free allocated receive buffer */
  LCIXC_lcoll_sched(coll, LCIXC_COLL_FREE, 0, receive, receive);

  *collp = coll;
  return LCI_OK;
}
