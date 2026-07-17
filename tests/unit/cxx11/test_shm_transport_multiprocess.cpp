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
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr int max_progress_iters = 1000000;

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

void progress_device(lci::device_t device)
{
  lci::progress_x().device(device)();
}

void bootstrap_barrier() { LCT_pmi_barrier(); }

template <typename Fn>
lci::status_t retry_until_on_device(lci::device_t device, Fn&& fn)
{
  for (int i = 0; i < max_progress_iters; ++i) {
    lci::status_t status = fn();
    if (!status.is_retry()) return status;
    progress_device(device);
  }
  std::fprintf(stderr, "LCI device operation did not leave retry state\n");
  std::abort();
}

void exchange_on_device(lci::device_t device, lci::tag_t tag, int salt)
{
  const int rank = lci::get_rank_me();
  const int peer = 1 - rank;
  const size_t msg_size = 32;
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  std::vector<char> send_buffer(msg_size);
  std::vector<char> recv_buffer(msg_size);
  fill_pattern(send_buffer, rank, salt);

  lci::status_t recv_status = retry_until_on_device(device, [&] {
    return lci::post_recv_x(peer, recv_buffer.data(), msg_size, tag, rcq)
        .device(device)();
  });
  lci::status_t send_status = retry_until_on_device(device, [&] {
    return lci::post_send_x(peer, send_buffer.data(), msg_size, tag, scq)
        .device(device)();
  });
  assert(recv_status.is_done() || recv_status.is_posted());
  assert(send_status.is_done() || send_status.is_posted());

  for (int i = 0; i < max_progress_iters &&
                  (recv_status.is_posted() || send_status.is_posted());
       ++i) {
    if (recv_status.is_posted()) {
      lci::status_t status = lci::cq_pop(rcq);
      if (status.is_done()) recv_status = status;
    }
    if (send_status.is_posted()) {
      lci::status_t status = lci::cq_pop(scq);
      if (status.is_done()) send_status = status;
    }
    progress_device(device);
  }
  assert(recv_status.is_done());
  assert(send_status.is_done());
  check_pattern(recv_buffer, peer, salt);

  lci::free_comp(&rcq);
  lci::free_comp(&scq);
}

void run_multi_device_runtime()
{
  assert(lci::get_rank_n() == 2);
  lci::runtime_t runtime = lci::get_g_runtime();
  lci::device_t first =
      lci::alloc_device_x().runtime(runtime).shm_enable(true)();
  lci::device_t second =
      lci::alloc_device_x().runtime(runtime).shm_enable(true)();
  assert(lci::shm::is_enabled(first.get_impl()->shm_device));
  assert(lci::shm::is_enabled(second.get_impl()->shm_device));

  exchange_on_device(first, 7001, 101);
  exchange_on_device(second, 7002, 202);

  lci::free_device_x(&second).runtime(runtime)();
  lci::free_device_x(&first).runtime(runtime)();
}

bool shm_enabled()
{
  auto device = lci::get_default_device();
  return lci::shm::is_enabled(device.get_impl()->shm_device);
}

void run_posted_alltoall(lci::comp_semantic_t comp_semantic =
                             lci::comp_semantic_t::memory)
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  const size_t msg_size = 32;
  const lci::tag_t tag = 1000;
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
                              scq)
          .comp_semantic(comp_semantic)();
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

}

void run_unexpected_pair()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  assert(nranks >= 2);
  constexpr int sender = 0;
  constexpr int receiver = 1;
  const size_t msg_size = 24;
  const lci::tag_t tag = 2000;
  bootstrap_barrier();
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  std::vector<char> send_buffer(msg_size);
  std::vector<char> recv_buffer(msg_size);
  fill_pattern(send_buffer, sender, receiver);

  lci::status_t send_status;
  if (rank == sender) {
    send_status = retry_until([&] {
      return lci::post_send_x(receiver, send_buffer.data(), msg_size, tag, scq)
          .allow_retry(false)();
    });
    assert(send_status.is_done() || send_status.is_posted());
  }
  bootstrap_barrier();

  lci::status_t recv_status;
  if (rank == receiver) {
    bool unexpected_received = false;
    for (int i = 0; i < max_progress_iters; ++i) {
      if (lci::progress().is_done()) {
        unexpected_received = true;
        break;
      }
    }
    if (!unexpected_received) {
      std::fprintf(stderr,
                   "SHM packet did not arrive before posting receive\n");
      std::abort();
    }
    recv_status = retry_until([&] {
      return lci::post_recv_x(sender, recv_buffer.data(), msg_size, tag, rcq)();
    });
  }
  if (rank == sender && send_status.is_posted()) {
    cq_pop_until(scq);
  }
  if (rank == receiver && recv_status.is_posted()) {
    recv_status = cq_pop_until(rcq);
  }
  if (rank == receiver) {
    assert(recv_status.is_done());
    check_pattern(recv_buffer, sender, receiver);
  }
  lci::free_comp(&rcq);
  lci::free_comp(&scq);

  bootstrap_barrier();
  assert(shm_enabled());
}

void run_active_message()
{
  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();
  assert(nranks >= 2);
  constexpr int sender = 0;
  constexpr int receiver = 1;
  const size_t msg_size = 16;
  const lci::tag_t tag = 3000;
  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  bootstrap_barrier();
  std::vector<char> send_buffer(msg_size);
  fill_pattern(send_buffer, sender, receiver);

  if (rank == sender) {
    lci::status_t send_status = retry_until([&] {
      return lci::post_am_x(receiver, send_buffer.data(), msg_size, lcq, rcomp)
          .tag(tag)
          .allow_retry(false)();
    });
    assert(send_status.is_done() || send_status.is_posted());
    if (send_status.is_posted()) {
      cq_pop_until(lcq);
    }
  }
  if (rank == receiver) {
    lci::status_t recv_status = cq_pop_until(rcq);
    assert(recv_status.rank == sender);
    assert(recv_status.size == msg_size);
    const char* payload = static_cast<const char*>(recv_status.buffer);
    for (size_t i = 0; i < msg_size; ++i) {
      assert(payload[i] ==
             static_cast<char>('A' + ((sender + receiver + i) % 26)));
    }
    std::free(recv_status.buffer);
  }
  bootstrap_barrier();
  assert(shm_enabled());

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

  assert(shm_enabled());
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

  assert(shm_enabled());
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

  assert(shm_enabled());
}

void run_enabled_suite()
{
  assert(shm_enabled());
  run_posted_alltoall();
  run_posted_alltoall(lci::comp_semantic_t::network);
  run_unexpected_pair();
  run_active_message();
}

void run_disabled_mode()
{
  auto shm_device = lci::get_default_device().get_impl()->shm_device;
  assert(!shm_device.is_empty());
  assert(!lci::shm::is_enabled(shm_device));
  run_posted_alltoall();
}
}  // namespace

int main(int argc, char** argv)
{
  const char* configured_mode = std::getenv("LCI_SHM_TEST_MODE");
  const std::string mode = configured_mode != nullptr
                               ? configured_mode
                               : (argc >= 2 ? argv[1] : "alltoall");
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
  } else if (mode == "multi-device") {
    run_multi_device_runtime();
  } else if (mode == "disabled") {
    run_disabled_mode();
  } else if (mode == "fallback") {
    run_ring_full_fallback();
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
