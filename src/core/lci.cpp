// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
const char* errorcode_to_str(errorcode_t errorcode)
{
  static const char errorcode_str[][32] = {
      "ok_min",         "ok",          "ok_backlog",     "ok_max",
      "posted_min",     "posted",      "posted_backlog", "posted_max",
      "retry_min",      "retry",       "retry_init",     "retry_lock",
      "retry_nopacket", "retry_nomem", "retry_backlog",  "retry_max",
      "fatal",
  };
  return errorcode_str[static_cast<int>(errorcode)];
}

}  // namespace lci