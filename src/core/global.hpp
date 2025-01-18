#ifndef LCIXX_CORE_GLOBAL_HPP
#define LCIXX_CORE_GLOBAL_HPP

namespace lcixx
{
extern int g_rank, g_nranks;
extern global_attr_t g_default_attr;
extern runtime_t g_default_runtime;

void global_initialize();
void global_finalize();
}  // namespace lcixx

#endif  // LCIXX_CORE_GLOBAL_HPP