#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pmi_wrapper.hpp"
#include "pmix.h"

namespace lct
{
namespace pmi
{
namespace pmix
{
#define PMIX_SAFECALL(x)                                                    \
  {                                                                         \
    int err = (x);                                                          \
    if (err != PMIX_SUCCESS) {                                              \
      fprintf(stderr, "err %d : %s (%s:%d)\n", err, PMIx_Error_string(err), \
              __FILE__, __LINE__);                                          \
      exit(1);                                                              \
    }                                                                       \
  }                                                                         \
  while (0)

pmix_proc_t proc_me;
pmix_proc_t proc_wild;

int check_availability()
{
  char* p = getenv("PMIX_RANK");
  if (p)
    return true;
  else
    return false;
}

void initialize()
{
  PMIX_SAFECALL(PMIx_Init(&proc_me, nullptr, 0));
  PMIX_PROC_CONSTRUCT(&proc_wild);
  PMIX_LOAD_PROCID(&proc_wild, proc_me.nspace, PMIX_RANK_WILDCARD);
}

int initialized()
{
  int initialized = PMIx_Initialized();
  return initialized;
}

int get_rank() { return (int)proc_me.rank; }

int get_size()
{
  pmix_value_t* val = nullptr;
  PMIX_SAFECALL(PMIx_Get(&proc_wild, PMIX_JOB_SIZE, nullptr, 0, &val));
  int nprocs = (int)val->data.uint32;
  PMIX_VALUE_RELEASE(val);
  return nprocs;
}

void publish(char* key, char* value)
{
  pmix_value_t val;
  val.type = PMIX_STRING;
  val.data.string = value;
  PMIX_SAFECALL(PMIx_Put(PMIX_GLOBAL, key, &val));
  PMIX_SAFECALL(PMIx_Commit());
}

void getname(int rank, char* key, char* value)
{
  pmix_value_t* val;
  proc_wild.rank = rank;
  PMIX_SAFECALL(PMIx_Get(&proc_wild, key, nullptr, 0, &val));
  proc_wild.rank = PMIX_RANK_WILDCARD;
  int n = snprintf(value, LCT_PMI_STRING_LIMIT + 1, "%s", val->data.string);
  LCT_Assert(LCT_log_ctx_default, 0 < n && n <= LCT_PMI_STRING_LIMIT,
             "snprintf failed (return %d)!\n", n);
  PMIX_VALUE_RELEASE(val);
}

void barrier()
{
  pmix_info_t* info;
  PMIX_INFO_CREATE(info, 1);
  bool flag = true;
  PMIX_INFO_LOAD(info, PMIX_COLLECT_DATA, &flag, PMIX_BOOL);
  PMIX_SAFECALL(PMIx_Fence(&proc_wild, 1, info, 1));
  PMIX_INFO_FREE(info, 1);
}

void finalize() { PMIX_SAFECALL(PMIx_Finalize(nullptr, 0)); }

}  // namespace pmix

void pmix_setup_ops(struct ops_t* ops)
{
  ops->check_availability = pmix::check_availability;
  ops->initialize = pmix::initialize;
  ops->is_initialized = pmix::initialized;
  ops->get_rank = pmix::get_rank;
  ops->get_size = pmix::get_size;
  ops->publish = pmix::publish;
  ops->getname = pmix::getname;
  ops->barrier = pmix::barrier;
  ops->finalize = pmix::finalize;
}
}  // namespace pmi
}  // namespace lct