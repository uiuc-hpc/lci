#ifndef LCI_CORE_GLOBAL_HPP
#define LCI_CORE_GLOBAL_HPP

namespace lci
{
extern int g_rank, g_nranks;
extern global_attr_t g_default_attr;
extern runtime_t g_default_runtime;

void global_initialize();
void global_finalize();
}  // namespace lci

#endif  // LCI_CORE_GLOBAL_HPP