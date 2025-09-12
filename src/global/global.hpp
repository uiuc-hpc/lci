// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_CORE_GLOBAL_HPP
#define LCI_CORE_GLOBAL_HPP

namespace lci
{
extern int g_rank_me, g_rank_n;
extern runtime_t g_default_runtime;
// Internal global configuration
namespace internal_config
{
extern bool enable_bootstrap_lci;
}  // namespace internal_config
}  // namespace lci

#endif  // LCI_CORE_GLOBAL_HPP