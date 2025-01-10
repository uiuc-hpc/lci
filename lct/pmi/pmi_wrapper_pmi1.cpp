#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.hpp"
#include "pmi.h"

namespace lct
{
namespace pmi
{
namespace pmi1
{
int check_availability()
{
  char* p = getenv("PMI_RANK");
  if (p)
    return true;
  else
    return false;
}

void initialize()
{
  int spawned;
  PMI_Init(&spawned);
}

int initialized()
{
  int initialized;
  PMI_Initialized(&initialized);
  return initialized;
}
int get_rank()
{
  int rank;
  PMI_Get_rank(&rank);
  return rank;
}

int get_size()
{
  int size;
  PMI_Get_size(&size);
  return size;
}

void publish(char* key, char* value)
{
  char lcg_name[LCT_PMI_STRING_LIMIT + 1];
  PMI_KVS_Get_my_name(lcg_name, LCT_PMI_STRING_LIMIT);
  PMI_KVS_Put(lcg_name, key, value);
}

void getname(int rank, char* key, char* value)
{
  char lcg_name[LCT_PMI_STRING_LIMIT + 1];
  PMI_KVS_Get_my_name(lcg_name, LCT_PMI_STRING_LIMIT);
  PMI_KVS_Get(lcg_name, key, value, LCT_PMI_STRING_LIMIT);
}

void barrier() { PMI_Barrier(); }

void finalize() { PMI_Finalize(); }

}  // namespace pmi1

void pmi1_setup_ops(struct ops_t* ops)
{
  ops->check_availability = pmi1::check_availability;
  ops->initialize = pmi1::initialize;
  ops->is_initialized = pmi1::initialized;
  ops->get_rank = pmi1::get_rank;
  ops->get_size = pmi1::get_size;
  ops->publish = pmi1::publish;
  ops->getname = pmi1::getname;
  ops->barrier = pmi1::barrier;
  ops->finalize = pmi1::finalize;
}
}  // namespace pmi
}  // namespace lct