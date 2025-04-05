// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_BOOTSTRAP_HPP
#define LCI_BOOTSTRAP_HPP

namespace lci
{
namespace bootstrap
{
void initialize();
int get_rank_me();
int get_rank_n();
void finalize();
void set_device(device_t device);
void alltoall(const void* sendbuf, void* recvbuf, size_t count);
}  // namespace bootstrap
}  // namespace lci

#endif  // LCI_BOOTSTRAP_HPP