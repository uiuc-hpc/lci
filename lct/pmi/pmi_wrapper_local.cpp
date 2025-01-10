#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pmi_wrapper.hpp"
#include "pmii_archive.hpp"

namespace lct
{
namespace pmi
{
namespace local
{
static int is_initialized = 0;
static archive::Archive_t l_archive;

int check_availability()
{
#ifndef LCT_PMI_BACKEND_ENABLE_PMIX
  if (getenv("PMIX_RANK"))
    LCT_Warn(
        LCT_log_ctx_default,
        "LCT detects the PMIx environment. However, the LCT PMIx support is "
        "not enabled. LCT assumes the number of processes of this job is 1. If "
        "you intended to run more than one processes, please do one of the "
        "followings:\n"
        "\t(a) set up the PMI2 environment by `srun --mpi=pmi2 "
        "[application]`.\n"
        "\t(b) set PMIX_ROOT when configuring LCT to enable the PMIx support.\n"
        "\t(c) set MPI_ROOT when configuring LCT to enable the MPI support "
        "(last resort).\n");
#endif
  return true;
}

void initialize()
{
  archive::init(&l_archive);
  is_initialized = 1;
}

int initialized() { return is_initialized; }
int get_rank() { return 0; }

int get_size() { return 1; }

void publish(char* key, char* value) { archive::push(&l_archive, key, value); }

void getname(int rank, char* key, char* value)
{
  char* ret = archive::search(&l_archive, key);
  strcpy(value, ret);
}

void barrier() {}

void finalize()
{
  archive::fina(&l_archive);
  is_initialized = 0;
}

}  // namespace local

void local_setup_ops(struct ops_t* ops)
{
  ops->check_availability = local::check_availability;
  ops->initialize = local::initialize;
  ops->is_initialized = local::initialized;
  ops->get_rank = local::get_rank;
  ops->get_size = local::get_size;
  ops->publish = local::publish;
  ops->getname = local::getname;
  ops->barrier = local::barrier;
  ops->finalize = local::finalize;
}
}  // namespace pmi
}  // namespace lct