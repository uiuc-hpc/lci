// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
void comp_signal_x::call_impl(comp_t comp, status_t status,
                              runtime_t runtime) const
{
  comp.p_impl->signal(status);
}

}  // namespace lci
