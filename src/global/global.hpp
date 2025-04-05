// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_CORE_GLOBAL_HPP
#define LCI_CORE_GLOBAL_HPP

namespace lci
{
extern int g_rank, g_nranks;
extern global_attr_t g_default_attr;
extern runtime_t g_default_runtime;
// Internal global configuration
namespace internal_config
{
extern bool enable_bootstrap_lci;
}  // namespace internal_config

void global_initialize();
void global_finalize();
}  // namespace lci

#endif  // LCI_CORE_GLOBAL_HPP