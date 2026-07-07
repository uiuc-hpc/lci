// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci.hpp"
#include "lci_internal.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr int max_progress_iters = 1000000;

// Minimal core-device shim for exercising the production shm::alloc_device()
// lifecycle without requiring an IBV/OFI runtime backend on the test host.
// SHM setup only needs a non-empty packet pool to derive packet payload size.
class fake_net_context_impl_t : public lci::net_context_impl_t
{
 public:
  fake_net_context_impl_t() : lci::net_context_impl_t(lci::runtime_t(), attr())
  {
  }

  lci::device_t alloc_device(lci::device_t::attr_t) override
  {
    return lci::device_t();
  }

 private:
  static lci::net_context_t::attr_t attr()
  {
    lci::net_context_t::attr_t value = {};
    value.backend = lci::attr_backend_t::none;
    value.name = "shm-direct-test-net-context";
    return value;
  }
};

class fake_device_impl_t : public lci::device_impl_t
{
 public:
  fake_device_impl_t(lci::net_context_t context, lci::packet_pool_t packet_pool)
      : lci::device_impl_t(context, attr())
  {
    this->packet_pool = packet_pool;
  }

  lci::endpoint_t alloc_endpoint_impl(lci::endpoint_t::attr_t) override
  {
    return lci::endpoint_t();
  }
  lci::mr_t register_memory_impl(void*, size_t) override { return lci::mr_t(); }
  void deregister_memory_impl(lci::mr_impl_t*) override {}
  uint64_t get_rkey(lci::mr_impl_t*) override { return 0; }
  size_t poll_comp_impl(lci::net_status_t*, size_t) override { return 0; }
  lci::error_t post_recv_impl(void*, size_t, lci::mr_t, void*) override
  {
    return lci::errorcode_t::fatal;
  }
  size_t post_recvs_impl(void*[], size_t, size_t, lci::mr_t, void*[]) override
  {
    return 0;
  }

 private:
  static lci::device_t::attr_t attr()
  {
    lci::device_t::attr_t value = {};
    value.name = "shm-direct-test-device";
    value.shm_enable = true;
    return value;
  }
};

class fake_core_device_t
{
 public:
  fake_core_device_t()
  {
    lci::packet_pool_attr_t packet_pool_attr = {};
    packet_pool_attr.packet_size = 512;
    packet_pool_attr.npackets = 0;
    packet_pool_attr.name = "shm-direct-test-packet-pool";
    packet_pool.p_impl = new lci::packet_pool_impl_t(packet_pool_attr);

    net_context_impl.reset(new fake_net_context_impl_t);
    device_impl.reset(
        new fake_device_impl_t(net_context_impl->net_context, packet_pool));
    device = device_impl->device;
  }

  ~fake_core_device_t()
  {
    device = lci::device_t();
    device_impl.reset();
    net_context_impl.reset();
    delete packet_pool.p_impl;
    packet_pool.p_impl = nullptr;
  }

  lci::device_t device;

 private:
  lci::packet_pool_t packet_pool;
  std::unique_ptr<fake_net_context_impl_t> net_context_impl;
  std::unique_ptr<fake_device_impl_t> device_impl;
};

template <typename Fn>
lci::status_t retry_until(Fn&& fn)
{
  for (int i = 0; i < max_progress_iters; ++i) {
    lci::status_t status = fn();
    if (!status.is_retry()) return status;
    lci::progress();
  }
  std::fprintf(stderr, "LCI operation did not leave retry state\n");
  std::abort();
}

lci::status_t cq_pop_until(lci::comp_t cq)
{
  for (int i = 0; i < max_progress_iters; ++i) {
    lci::status_t status = lci::cq_pop(cq);
    if (status.is_done()) return status;
    assert(status.is_retry());
    lci::progress();
  }
  std::fprintf(stderr, "LCI completion did not arrive\n");
  std::abort();
}

void fill_pattern(std::vector<char>& buffer, int rank, int salt)
{
  for (size_t i = 0; i < buffer.size(); ++i) {
    buffer[i] = static_cast<char>('A' + ((rank + salt + i) % 26));
  }
}

void check_pattern(const std::vector<char>& buffer, int rank, int salt)
{
  for (size_t i = 0; i < buffer.size(); ++i) {
    assert(buffer[i] == static_cast<char>('A' + ((rank + salt + i) % 26)));
  }
}

void run_route_agreement_bootstrap_only()
{
  LCT_init();
  lci::bootstrap::initialize();
  const int rank = lci::bootstrap::get_rank_me();
  const int nranks = lci::bootstrap::get_rank_n();
  assert(nranks >= 2);

  std::vector<uint8_t> local_routes(static_cast<size_t>(nranks), 1);
  if (rank == 0) local_routes[1] = 0;

  std::vector<uint8_t> send_matrix(static_cast<size_t>(nranks) * nranks, 0);
  for (int dst = 0; dst < nranks; ++dst) {
    std::copy(local_routes.begin(), local_routes.end(),
              send_matrix.begin() + static_cast<size_t>(dst) * nranks);
  }
  std::vector<uint8_t> recv_matrix(static_cast<size_t>(nranks) * nranks, 0);
  lci::bootstrap::alltoall(send_matrix.data(), recv_matrix.data(),
                           static_cast<size_t>(nranks));

  std::vector<uint8_t> agreed(static_cast<size_t>(nranks), 0);
  for (int peer = 0; peer < nranks; ++peer) {
    agreed[peer] = local_routes[peer] &&
                   recv_matrix[static_cast<size_t>(peer) * nranks + rank];
  }

  if (rank == 0) assert(agreed[1] == 0);
  if (rank == 1) assert(agreed[0] == 0);
  for (int peer = 2; peer < nranks; ++peer) {
    assert(agreed[peer] == 1);
  }

  lci::bootstrap::finalize();
  LCT_fina();
}

void force_split_topology(lci::shm::context_t context)
{
  // Simulate a non-uniform node layout: ranks 0/1 have a local peer, while
  // rank 2 is a singleton. A rank-local early return in alloc/free deadlocks
  // this test because only ranks 0/1 would enter the global bootstrap rounds.
  auto* impl = context.get_impl();
  const int rank = impl->global_rank;
  const int nranks = impl->global_size;
  assert(nranks == 3);

  impl->global_to_local.assign(static_cast<size_t>(nranks), -1);
  impl->local_to_global.clear();
  if (rank < 2) {
    impl->local_to_global.push_back(0);
    impl->local_to_global.push_back(1);
    impl->global_to_local[0] = 0;
    impl->global_to_local[1] = 1;
    impl->local_rank = rank;
  } else {
    impl->local_to_global.push_back(rank);
    impl->global_to_local[rank] = 0;
    impl->local_rank = 0;
  }
  impl->local_size = static_cast<int>(impl->local_to_global.size());
}

void run_split_topology_lifecycle_direct()
{
  LCT_init();
  lci::bootstrap::initialize();
  const int rank = lci::bootstrap::get_rank_me();
  const int nranks = lci::bootstrap::get_rank_n();
  assert(nranks == 3);

  lci::shm::context_t context = lci::shm::alloc_context(lci::runtime_t(), true);
  force_split_topology(context);
  fake_core_device_t core_device;

  lci::shm::device_t shm_device = lci::shm::alloc_device(
      context, core_device.device, true, 4096, 256, 64, 1);

  if (rank < 2) {
    assert(lci::shm::is_enabled(shm_device));
    assert(lci::shm::can_send(shm_device, 1 - rank, 32));
    assert(!lci::shm::can_send(shm_device, 2, 32));
  } else {
    assert(!lci::shm::is_enabled(shm_device));
    for (int peer = 0; peer < nranks; ++peer) {
      assert(!lci::shm::can_send(shm_device, peer, 32));
    }
  }

  lci::shm::free_device(&shm_device);
  lci::shm::free_context(&context);
  lci::bootstrap::finalize();
  LCT_fina();
}

lci::shm::counters_t shm_counters()
{
  auto device = lci::get_default_device();
  return lci::shm::get_counters(device.get_impl()->shm_device);
}

bool shm_enabled()
{
  auto device = lci::get_default_device();
  return lci::shm::is_enabled(device.get_impl()->shm_device);
}

void run_posted_alltoall(bool expect_shm)
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  const size_t msg_size = 32;
  const lci::tag_t tag = 1000;
  auto before = shm_counters();

  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  std::vector<std::vector<char>> send_buffers(nranks,
                                              std::vector<char>(msg_size));
  std::vector<std::vector<char>> recv_buffers(nranks,
                                              std::vector<char>(msg_size));
  std::vector<char> recv_done(nranks, 0);
  int recv_remaining = nranks;
  int send_remaining = 0;

  for (int peer = 0; peer < nranks; ++peer) {
    fill_pattern(send_buffers[peer], rank, peer);
    lci::status_t status = retry_until([&] {
      return lci::post_recv_x(peer, recv_buffers[peer].data(), msg_size, tag,
                              rcq)();
    });
    if (status.is_done()) {
      recv_done[peer] = 1;
      --recv_remaining;
    } else {
      assert(status.is_posted());
    }
  }

  for (int peer = 0; peer < nranks; ++peer) {
    lci::status_t status = retry_until([&] {
      return lci::post_send_x(peer, send_buffers[peer].data(), msg_size, tag,
                              scq)();
    });
    if (status.is_posted()) ++send_remaining;
    assert(status.is_done() || status.is_posted());
  }

  while (recv_remaining > 0 || send_remaining > 0) {
    if (send_remaining > 0) {
      lci::status_t status = lci::cq_pop(scq);
      if (status.is_done()) --send_remaining;
    }
    if (recv_remaining > 0) {
      lci::status_t status = lci::cq_pop(rcq);
      if (status.is_done()) {
        assert(status.rank >= 0 && status.rank < nranks);
        if (!recv_done[status.rank]) {
          recv_done[status.rank] = 1;
          --recv_remaining;
        }
      }
    }
    lci::progress();
  }

  for (int peer = 0; peer < nranks; ++peer) {
    check_pattern(recv_buffers[peer], peer, rank);
  }
  lci::free_comp(&rcq);
  lci::free_comp(&scq);

  auto after = shm_counters();
  if (expect_shm) {
    assert(shm_enabled());
    assert(after.send_messages > before.send_messages);
    assert(after.recv_messages > before.recv_messages);
  } else {
    assert(!shm_enabled());
    assert(after.send_messages == before.send_messages);
    assert(after.recv_messages == before.recv_messages);
  }
}

void run_unexpected_pair()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  const int dst = (rank + 1) % nranks;
  const int src = (rank + nranks - 1) % nranks;
  const size_t msg_size = 24;
  const lci::tag_t tag = 2000;
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  std::vector<char> send_buffer(msg_size);
  std::vector<char> recv_buffer(msg_size);
  fill_pattern(send_buffer, rank, dst);

  lci::status_t send_status = retry_until([&] {
    return lci::post_send_x(dst, send_buffer.data(), msg_size, tag, scq)
        .allow_retry(false)();
  });
  assert(send_status.is_done() || send_status.is_posted());
  lci::barrier();

  lci::status_t recv_status = retry_until([&] {
    return lci::post_recv_x(src, recv_buffer.data(), msg_size, tag, rcq)();
  });
  if (send_status.is_posted()) {
    cq_pop_until(scq);
  }
  if (recv_status.is_posted()) {
    recv_status = cq_pop_until(rcq);
  }
  assert(recv_status.is_done());
  check_pattern(recv_buffer, src, rank);
  lci::free_comp(&rcq);
  lci::free_comp(&scq);
}

void run_active_message()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  const int dst = (rank + 1) % nranks;
  const int src = (rank + nranks - 1) % nranks;
  const size_t msg_size = 16;
  const lci::tag_t tag = 3000;
  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  lci::barrier();
  std::vector<char> send_buffer(msg_size);
  fill_pattern(send_buffer, rank, dst);

  lci::status_t send_status = retry_until([&] {
    return lci::post_am_x(dst, send_buffer.data(), msg_size, lcq, rcomp)
        .tag(tag)
        .allow_retry(false)();
  });
  assert(send_status.is_done() || send_status.is_posted());
  if (send_status.is_posted()) {
    cq_pop_until(lcq);
  }
  lci::status_t recv_status = cq_pop_until(rcq);
  assert(recv_status.rank == src);
  assert(recv_status.size == msg_size);
  const char* payload = static_cast<const char*>(recv_status.buffer);
  for (size_t i = 0; i < msg_size; ++i) {
    assert(payload[i] == static_cast<char>('A' + ((src + rank + i) % 26)));
  }
  std::free(recv_status.buffer);
  lci::barrier();
  lci::deregister_rcomp(rcomp);
  lci::free_comp(&rcq);
  lci::free_comp(&lcq);
}

void run_ring_full_fallback()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  assert(nranks == 2);
  const int nmsgs = 32;
  const size_t msg_size = 8;
  const lci::tag_t tag_base = 4000;
  lci::comp_t cq = lci::alloc_cq();
  auto before = shm_counters();

  if (rank == 0) {
    std::vector<std::vector<char>> send_buffers(nmsgs,
                                                std::vector<char>(msg_size));
    for (int i = 0; i < nmsgs; ++i) {
      fill_pattern(send_buffers[i], rank, i);
      lci::status_t status =
          lci::post_send_x(1, send_buffers[i].data(), msg_size,
                           tag_base + static_cast<lci::tag_t>(i), cq)
              .allow_retry(false)();
      assert(status.is_done() || status.is_posted());
      if (status.is_posted()) cq_pop_until(cq);
    }
    auto after = shm_counters();
    assert(after.send_messages > before.send_messages);
    assert(after.retry_nomem > before.retry_nomem);
    assert(after.nic_fallbacks > before.nic_fallbacks);
  } else {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<std::vector<char>> recv_buffers(nmsgs,
                                                std::vector<char>(msg_size));
    std::vector<char> recv_done(nmsgs, 0);
    int remaining = nmsgs;
    for (int i = 0; i < nmsgs; ++i) {
      lci::status_t status = retry_until([&] {
        return lci::post_recv_x(0, recv_buffers[i].data(), msg_size,
                                tag_base + static_cast<lci::tag_t>(i), cq)();
      });
      if (status.is_done()) {
        recv_done[i] = 1;
        --remaining;
      } else {
        assert(status.is_posted());
      }
    }
    while (remaining > 0) {
      lci::status_t status = lci::cq_pop(cq);
      if (status.is_done()) {
        const int index = static_cast<int>(status.tag - tag_base);
        assert(index >= 0 && index < nmsgs);
        if (!recv_done[index]) {
          recv_done[index] = 1;
          --remaining;
        }
      }
      lci::progress();
    }
    for (int i = 0; i < nmsgs; ++i) {
      check_pattern(recv_buffers[i], 0, i);
    }
  }

  lci::barrier();
  lci::free_comp(&cq);
}

void run_route_disabled_fallback()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  assert(nranks == 2);
  const int peer = 1 - rank;
  const size_t msg_size = 32;
  const lci::tag_t tag = 5000;
  auto before = shm_counters();

  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  std::vector<char> send_buffer(msg_size);
  std::vector<char> recv_buffer(msg_size);
  fill_pattern(send_buffer, rank, peer);

  lci::status_t recv_status = retry_until([&] {
    return lci::post_recv_x(peer, recv_buffer.data(), msg_size, tag, rcq)();
  });
  lci::status_t send_status = retry_until([&] {
    return lci::post_send_x(peer, send_buffer.data(), msg_size, tag, scq)();
  });

  if (send_status.is_posted()) cq_pop_until(scq);
  if (recv_status.is_posted()) recv_status = cq_pop_until(rcq);
  assert(recv_status.is_done());
  check_pattern(recv_buffer, peer, rank);

  lci::free_comp(&rcq);
  lci::free_comp(&scq);

  auto after = shm_counters();
  assert(after.send_messages == before.send_messages);
  assert(after.recv_messages == before.recv_messages);
  if (shm_enabled()) {
    assert(after.nic_fallbacks > before.nic_fallbacks);
  }
}

void run_device_mr_fallback()
{
#if defined(LCI_USE_CUDA) || defined(LCI_USE_HIP)
  // A real accelerator-buffer test needs device allocation support from the
  // active backend. The non-accelerator build still exercises the explicit
  // MR_DEVICE eligibility guard without pretending a host vector is a real GPU
  // allocation in accelerator-enabled CI.
  std::fprintf(stderr,
               "Skipping MR_DEVICE fallback check in accelerator build\n");
#else
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  assert(nranks == 2);
  const int peer = 1 - rank;
  const size_t msg_size = 16;
  const lci::tag_t tag = 6000;
  auto before = shm_counters();

  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  std::vector<char> send_buffer(msg_size);
  std::vector<char> recv_buffer(msg_size);
  fill_pattern(send_buffer, rank, peer);

  lci::status_t recv_status = retry_until([&] {
    return lci::post_recv_x(peer, recv_buffer.data(), msg_size, tag, rcq)();
  });
  lci::status_t send_status = retry_until([&] {
    return lci::post_send_x(peer, send_buffer.data(), msg_size, tag, scq)
        .mr(lci::MR_DEVICE)();
  });

  if (send_status.is_posted()) cq_pop_until(scq);
  if (recv_status.is_posted()) recv_status = cq_pop_until(rcq);
  assert(recv_status.is_done());
  check_pattern(recv_buffer, peer, rank);

  lci::free_comp(&rcq);
  lci::free_comp(&scq);

  auto after = shm_counters();
  assert(shm_enabled());
  assert(after.send_messages == before.send_messages);
  assert(after.recv_messages == before.recv_messages);
  assert(after.nic_fallbacks > before.nic_fallbacks);
#endif
}

void run_large_tag_metadata()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  assert(nranks == 2);
  const int dst = (rank + 1) % nranks;
  const int src = (rank + nranks - 1) % nranks;
  const size_t msg_size = 20;
  const lci::tag_t tag =
      lci::get_g_runtime().get_attr_max_imm_tag() + static_cast<lci::tag_t>(1);
  auto before = shm_counters();

  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  std::vector<char> send_buffer(msg_size);
  std::vector<char> recv_buffer(msg_size);
  fill_pattern(send_buffer, rank, dst);

  lci::status_t recv_status = retry_until([&] {
    return lci::post_recv_x(src, recv_buffer.data(), msg_size, tag, rcq)();
  });
  lci::status_t send_status = retry_until([&] {
    return lci::post_send_x(dst, send_buffer.data(), msg_size, tag, scq)();
  });

  if (send_status.is_posted()) cq_pop_until(scq);
  if (recv_status.is_posted()) recv_status = cq_pop_until(rcq);
  assert(recv_status.is_done());
  assert(recv_status.rank == src);
  assert(recv_status.tag == tag);
  assert(recv_status.size == msg_size);
  check_pattern(recv_buffer, src, rank);

  lci::free_comp(&rcq);
  lci::free_comp(&scq);

  auto after = shm_counters();
  assert(shm_enabled());
  assert(after.send_messages > before.send_messages);
  assert(after.recv_messages > before.recv_messages);
}

void run_large_rcomp_metadata()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  assert(nranks == 2);
  const int dst = (rank + 1) % nranks;
  const int src = (rank + nranks - 1) % nranks;
  const size_t msg_size = 18;
  const lci::tag_t tag = 7;
  const lci::rcomp_t large_rcomp = static_cast<lci::rcomp_t>(
      lci::get_g_runtime().get_attr_max_imm_rcomp() + 1);
  auto before = shm_counters();

  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::reserve_rcomps(static_cast<lci::rcomp_t>(large_rcomp + 64));
  lci::register_rcomp_x(rcq).rcomp(large_rcomp)();
  lci::barrier();

  std::vector<char> send_buffer(msg_size);
  fill_pattern(send_buffer, rank, dst);
  lci::status_t send_status = retry_until([&] {
    return lci::post_am_x(dst, send_buffer.data(), msg_size, lcq, large_rcomp)
        .tag(tag)
        .allow_retry(false)();
  });
  assert(send_status.is_done() || send_status.is_posted());
  if (send_status.is_posted()) {
    cq_pop_until(lcq);
  }

  lci::status_t recv_status = cq_pop_until(rcq);
  assert(recv_status.rank == src);
  assert(recv_status.tag == tag);
  assert(recv_status.size == msg_size);
  const char* payload = static_cast<const char*>(recv_status.buffer);
  for (size_t i = 0; i < msg_size; ++i) {
    assert(payload[i] == static_cast<char>('A' + ((src + rank + i) % 26)));
  }
  std::free(recv_status.buffer);

  lci::barrier();
  lci::deregister_rcomp(large_rcomp);
  lci::free_comp(&rcq);
  lci::free_comp(&lcq);

  auto after = shm_counters();
  assert(shm_enabled());
  assert(after.send_messages > before.send_messages);
  assert(after.recv_messages > before.recv_messages);
}

void run_enabled_suite()
{
  run_posted_alltoall(true);
  run_unexpected_pair();
  run_active_message();
}
}  // namespace

int main(int argc, char** argv)
{
  const std::string mode = argc >= 2 ? argv[1] : "alltoall";
  if (mode == "route-agreement") {
    run_route_agreement_bootstrap_only();
    return 0;
  }
  if (mode == "split-lifecycle") {
    run_split_topology_lifecycle_direct();
    return 0;
  }
  try {
    if (mode == "large-tag" || mode == "large-rcomp") {
      lci::g_runtime_init_x().imm_nbits_tag(4).imm_nbits_rcomp(4)();
    } else {
      lci::g_runtime_init();
    }
  } catch (const std::exception& error) {
    std::fprintf(stderr,
                 "SHM transport multiprocess test runtime initialization "
                 "failed (%s)\n",
                 error.what());
    return 1;
  }
  if (mode == "alltoall") {
    run_enabled_suite();
  } else if (mode == "disabled") {
    run_posted_alltoall(false);
  } else if (mode == "fallback") {
    run_ring_full_fallback();
  } else if (mode == "route-disabled") {
    run_route_disabled_fallback();
  } else if (mode == "device-mr-fallback") {
    run_device_mr_fallback();
  } else if (mode == "large-tag") {
    run_large_tag_metadata();
  } else if (mode == "large-rcomp") {
    run_large_rcomp_metadata();
  } else {
    std::fprintf(stderr, "Unknown test mode '%s'\n", mode.c_str());
    lci::g_runtime_fina();
    return 2;
  }
  lci::g_runtime_fina();
  return 0;
}
