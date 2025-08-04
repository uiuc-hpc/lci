// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
const char* get_errorcode_str(errorcode_t errorcode)
{
  static const char errorcode_str[][32] = {
      "done_min",       "done",        "done_backlog",   "done_max",
      "posted_min",     "posted",      "posted_backlog", "posted_max",
      "retry_min",      "retry",       "retry_init",     "retry_lock",
      "retry_nopacket", "retry_nomem", "retry_backlog",  "retry_max",
      "fatal",
  };
  return errorcode_str[static_cast<int>(errorcode)];
}

const char* get_net_opcode_str(net_opcode_t opcode)
{
  static const char opcode_str[][16] = {
      "SEND", "RECV", "WRITE", "REMOTE_WRITE", "READ",
  };
  return opcode_str[static_cast<int>(opcode)];
}

const char* get_broadcast_algorithm_str(broadcast_algorithm_t algorithm)
{
  static const char algorithm_str[][8] = {"none", "direct", "tree", "ring"};
  return algorithm_str[static_cast<int>(algorithm)];
}

const char* get_reduce_scatter_algorithm_str(
    reduce_scatter_algorithm_t algorithm)
{
  static const char algorithm_str[][8] = {"none", "direct", "tree", "ring"};
  return algorithm_str[static_cast<int>(algorithm)];
}

const char* get_allreduce_algorithm_str(allreduce_algorithm_t algorithm)
{
  static const char algorithm_str[][8] = {"none", "direct", "tree", "ring"};
  return algorithm_str[static_cast<int>(algorithm)];
}

}  // namespace lci