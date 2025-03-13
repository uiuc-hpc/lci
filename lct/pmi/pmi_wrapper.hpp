#ifndef LCT_PMI_WRAPPER_HPP
#define LCT_PMI_WRAPPER_HPP
#include "lcti.hpp"

namespace lct
{
namespace pmi
{
struct ops_t {
  int (*check_availability)();
  void (*initialize)();
  int (*is_initialized)();
  int (*get_rank)();
  int (*get_size)();
  void (*publish)(char* key, char* value);
  void (*getname)(int rank, char* key, char* value);
  void (*barrier)();
  void (*finalize)();
};

void local_setup_ops(struct ops_t* ops);
void file_setup_ops(struct ops_t* ops);

#ifdef LCT_PMI_BACKEND_ENABLE_PMI1
void pmi1_setup_ops(struct ops_t* ops);
#endif

#ifdef LCT_PMI_BACKEND_ENABLE_PMI2
void pmi2_setup_ops(struct ops_t* ops);
#endif

#ifdef LCT_PMI_BACKEND_ENABLE_PMIX
void pmix_setup_ops(struct ops_t* ops);
#endif

#ifdef LCT_PMI_BACKEND_ENABLE_MPI
void mpi_setup_ops(struct ops_t* ops);
#endif

}  // namespace pmi
}  // namespace lct
#endif
