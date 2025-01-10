#include <stdlib.h>
#include <stdio.h>
#include "pmi_wrapper.hpp"
#include "pmi2.h"

namespace lct
{
namespace pmi
{
namespace pmi2
{
int g_rank, g_size;

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
  int spawned, appnum;
  PMI2_Init(&spawned, &g_size, &g_rank, &appnum);
}

int initialized() { return PMI2_Initialized(); }
int get_rank() { return g_rank; }

int get_size() { return g_size; }

void publish(char* key, char* value) { PMI2_KVS_Put(key, value); }

void getname(int rank, char* key, char* value)
{
  int vallen;
  PMI2_KVS_Get(nullptr, rank /* PMI2_ID_NULL */, key, value,
               LCT_PMI_STRING_LIMIT, &vallen);
}

void barrier()
{
  // WARNING: Switching to PMI2 breaks this barrier
  PMI2_KVS_Fence();
}

void finalize() { PMI2_Finalize(); }

}  // namespace pmi2

void pmi2_setup_ops(struct ops_t* ops)
{
  ops->check_availability = pmi2::check_availability;
  ops->initialize = pmi2::initialize;
  ops->is_initialized = pmi2::initialized;
  ops->get_rank = pmi2::get_rank;
  ops->get_size = pmi2::get_size;
  ops->publish = pmi2::publish;
  ops->getname = pmi2::getname;
  ops->barrier = pmi2::barrier;
  ops->finalize = pmi2::finalize;
}
}  // namespace pmi
}  // namespace lct