#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pmi_wrapper.h"
#include "pmix.h"

#define PMIX_SAFECALL(x)                                                    \
  {                                                                         \
    int err = (x);                                                          \
    if (err != PMIX_SUCCESS) {                                              \
      fprintf(stderr, "err %d : %s (%s:%d)\n", err, PMIx_Error_string(err), \
              __FILE__, __LINE__);                                          \
      exit(1);                                                              \
    }                                                                       \
  }                                                                         \
  while (0)                                                                 \
    ;

pmix_proc_t proc_me;
pmix_proc_t proc_wild;

void lcm_pm_pmix_initialize()
{
  PMIX_SAFECALL(PMIx_Init(&proc_me, NULL, 0));
  PMIX_PROC_CONSTRUCT(&proc_wild);
  PMIX_LOAD_PROCID(&proc_wild, proc_me.nspace, PMIX_RANK_WILDCARD);
}

int lcm_pm_pmix_initialized()
{
  int initialized = PMIx_Initialized();
  return initialized;
}

int lcm_pm_pmix_get_rank() { return (int)proc_me.rank; }

int lcm_pm_pmix_get_size()
{
  pmix_value_t* val = NULL;
  PMIX_SAFECALL(PMIx_Get(&proc_wild, PMIX_JOB_SIZE, NULL, 0, &val));
  int nprocs = (int)val->data.uint32;
  PMIX_VALUE_RELEASE(val);
  return nprocs;
}

void lcm_pm_pmix_publish(char* key, char* value)
{
  pmix_value_t val;
  val.type = PMIX_STRING;
  val.data.string = value;
  PMIX_SAFECALL(PMIx_Put(PMIX_GLOBAL, key, &val));
  PMIX_SAFECALL(PMIx_Commit());
}

void lcm_pm_pmix_getname(int rank, char* key, char* value)
{
  pmix_value_t* val;
  proc_wild.rank = rank;
  PMIX_SAFECALL(PMIx_Get(&proc_wild, key, NULL, 0, &val));
  proc_wild.rank = PMIX_RANK_WILDCARD;
  int n = snprintf(value, LCM_PMI_STRING_LIMIT + 1, "%s", val->data.string);
  LCM_Assert(0 < n && n <= LCM_PMI_STRING_LIMIT,
             "snprintf failed (return %d)!\n", n);
  PMIX_VALUE_RELEASE(val);
}

void lcm_pm_pmix_barrier()
{
  pmix_info_t* info;
  PMIX_INFO_CREATE(info, 1);
  bool flag = true;
  PMIX_INFO_LOAD(info, PMIX_COLLECT_DATA, &flag, PMIX_BOOL);
  PMIX_SAFECALL(PMIx_Fence(&proc_wild, 1, info, 1));
  PMIX_INFO_FREE(info, 1);
}

void lcm_pm_pmix_finalize() { PMIX_SAFECALL(PMIx_Finalize(NULL, 0)); }

void lcm_pm_pmix_setup_ops(struct LCM_PM_ops_t* ops)
{
  ops->initialize = lcm_pm_pmix_initialize;
  ops->is_initialized = lcm_pm_pmix_initialized;
  ops->get_rank = lcm_pm_pmix_get_rank;
  ops->get_size = lcm_pm_pmix_get_size;
  ops->publish = lcm_pm_pmix_publish;
  ops->getname = lcm_pm_pmix_getname;
  ops->barrier = lcm_pm_pmix_barrier;
  ops->finalize = lcm_pm_pmix_finalize;
}