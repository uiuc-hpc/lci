// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci.hpp"
#include "lci_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(LCI_USE_HIP)
#include <hip/hip_runtime.h>
#endif  // LCI_USE_HIP

namespace
{
constexpr int kSkipReturnCode = 77;
constexpr int kMaxPolls = 1000000;

enum class test_mode_t {
  expect_unsupported,
  require_ibv,
  require_ibv_gpu,
};

[[noreturn]] void fail(const char* message)
{
  std::fprintf(stderr, "network atomic test failure: %s\n", message);
  std::abort();
}

void check(bool condition, const char* message)
{
  if (!condition) {
    fail(message);
  }
}

void bootstrap_barrier() { LCT_pmi_barrier(); }

struct alignas(uint64_t) remote_words_t {
  uint64_t counter;
};

struct completion_context_t {
  lci::net_opcode_t opcode;
  bool seen = false;
};

void wait_for_completions(lci::runtime_t runtime, lci::device_t device,
                          const std::vector<completion_context_t*>& expected);
void wait_for_discarded_completion(lci::runtime_t runtime,
                                   lci::device_t device);

const char* atomic_scope_str(lci::net_atomic_scope_t scope)
{
  switch (scope) {
    case lci::net_atomic_scope_t::NONE:
      return "NONE";
    case lci::net_atomic_scope_t::HCA:
      return "HCA";
    case lci::net_atomic_scope_t::GLOBAL:
      return "GLOBAL";
    default:
      return "invalid";
  }
}

#if defined(LCI_USE_HIP)
#define HIP_CHECK(call)                                                   \
  do {                                                                    \
    hipError_t error = (call);                                            \
    if (error != hipSuccess) {                                            \
      std::fprintf(stderr, "HIP failure %s:%d: %s\n", __FILE__, __LINE__, \
                   hipGetErrorString(error));                             \
      std::abort();                                                       \
    }                                                                     \
  } while (0)

void run_gpu_atomic_test(lci::runtime_t runtime, lci::net_context_t net_context,
                         lci::device_t device, lci::endpoint_t endpoint,
                         int peer, lci::net_atomic_scope_t required_scope)
{
  constexpr uint64_t kInitialValue = 700;
  constexpr uint64_t kAddValue = 9;
  const int rank = lci::get_rank_me();

  uint64_t* target = nullptr;
  uint64_t* result = nullptr;
  HIP_CHECK(hipMalloc(reinterpret_cast<void**>(&target), sizeof(*target)));
  HIP_CHECK(hipMalloc(reinterpret_cast<void**>(&result), sizeof(*result)));
  HIP_CHECK(hipMemcpy(target, &kInitialValue, sizeof(kInitialValue),
                      hipMemcpyHostToDevice));
  HIP_CHECK(hipMemset(result, 0, sizeof(*result)));

  lci::mr_t target_mr = lci::register_memory_x(target, sizeof(*target))
                            .runtime(runtime)
                            .device(device)();
  lci::mr_t result_mr = lci::register_memory_x(result, sizeof(*result))
                            .runtime(runtime)
                            .device(device)();
  check(net_context.get_attr_use_dmabuf(),
        "HIP GPU atomic target/result registration did not use DMA-BUF");

  lci::rmr_t local_rmr = lci::get_rmr(target_mr);
  std::array<lci::rmr_t, 2> local_rmrs = {local_rmr, local_rmr};
  std::array<lci::rmr_t, 2> remote_rmrs = {};
  lci::bootstrap::alltoall(local_rmrs.data(), remote_rmrs.data(),
                           sizeof(local_rmr));

  bootstrap_barrier();
  if (rank == 0) {
    completion_context_t context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t error =
        lci::net_post_fetch_add_x(peer, result, result_mr, kAddValue, 0,
                                  remote_rmrs[peer])
            .runtime(runtime)
            .required_atomic_scope(required_scope)
            .device(device)
            .endpoint(endpoint)
            .user_context(&context)();
    check(error.is_posted(), "GPU fetch-add was not posted");
    wait_for_completions(runtime, device, {&context});

    uint64_t observed_result = 0;
    HIP_CHECK(hipMemcpy(&observed_result, result, sizeof(observed_result),
                        hipMemcpyDeviceToHost));
    check(observed_result == kInitialValue,
          "GPU fetch-add returned the wrong previous value");
  }
  bootstrap_barrier();
  if (rank == 1) {
    uint64_t observed_target = 0;
    HIP_CHECK(hipMemcpy(&observed_target, target, sizeof(observed_target),
                        hipMemcpyDeviceToHost));
    check(observed_target == kInitialValue + kAddValue,
          "GPU fetch-add did not update the remote GPU value");
  }
  bootstrap_barrier();

  lci::deregister_memory_x(&result_mr).runtime(runtime)();
  lci::deregister_memory_x(&target_mr).runtime(runtime)();
  HIP_CHECK(hipFree(result));
  HIP_CHECK(hipFree(target));
}
#endif  // LCI_USE_HIP

void wait_for_completions(lci::runtime_t runtime, lci::device_t device,
                          const std::vector<completion_context_t*>& expected)
{
  size_t completed = 0;
  for (int attempt = 0; attempt < kMaxPolls && completed < expected.size();
       ++attempt) {
    lci::net_status_t status = {};
    const size_t n =
        lci::net_poll_cq_x(1, &status).runtime(runtime).device(device)();
    if (n == 0) {
      continue;
    }
    check(n == 1, "net_poll_cq returned an unexpected completion count");
    check(status.opcode != lci::net_opcode_t::ERROR,
          "network atomic operation completed with an error");
    auto* context = static_cast<completion_context_t*>(status.user_context);
    bool matched = false;
    for (completion_context_t* candidate : expected) {
      if (candidate == context) {
        check(!candidate->seen, "network atomic completion was duplicated");
        check(status.opcode == candidate->opcode,
              "network atomic completion reported the wrong opcode");
        candidate->seen = true;
        ++completed;
        matched = true;
        break;
      }
    }
    check(matched, "network atomic completion had an unexpected context");
  }
  check(completed == expected.size(), "network atomic completion timed out");
}

void wait_for_discarded_completion(lci::runtime_t runtime, lci::device_t device)
{
  for (int attempt = 0; attempt < kMaxPolls; ++attempt) {
    const size_t n =
        lci::net_poll_cq_x(1, nullptr).runtime(runtime).device(device)();
    if (n == 1) {
      return;
    }
    check(n == 0, "net_poll_cq returned an unexpected completion count");
  }
  fail("discarded network atomic completion timed out");
}

void free_runtime_objects(lci::runtime_t runtime, lci::device_t* device,
                          lci::endpoint_t* endpoint)
{
  if (!endpoint->is_empty()) {
    lci::free_endpoint_x(endpoint).runtime(runtime)();
  }
  if (!device->is_empty()) {
    lci::free_device_x(device).runtime(runtime)();
  }
  lci::g_runtime_fina();
}

int run_test(test_mode_t mode)
{
  lci::runtime_t runtime = lci::g_runtime_init_x()
                               .alloc_default_device(false)
                               .alloc_default_packet_pool(false)
                               .alloc_default_matching_engine(false)();
  // This raw-RMA test deliberately avoids a packet pool. Keep QP/RMR
  // bootstrap on PMI so bootstrap::alltoall does not invoke LCI collectives
  // that require a bound packet pool.
  lci::internal_config::enable_bootstrap_lci = false;
  lci::net_context_t net_context =
      lci::get_default_net_context_x().runtime(runtime)();
  lci::device_t device = lci::alloc_device_x()
                             .runtime(runtime)
                             .net_max_sends(4)
                             .alloc_default_endpoint(false)
                             .alloc_progress_endpoint(false)
                             .shm_enable(false)();
  lci::endpoint_t endpoint =
      lci::alloc_endpoint_x().runtime(runtime).device(device)();

  const lci::net_atomic_scope_t atomic_scope =
      net_context.get_attr_atomic_scope();
  if (mode == test_mode_t::expect_unsupported) {
    check(atomic_scope == lci::net_atomic_scope_t::NONE,
          "OFI unexpectedly advertised uint64 fetch-add support");
    uint64_t result = 0;
    for (const lci::net_atomic_scope_t required_scope :
         {lci::net_atomic_scope_t::NONE, lci::net_atomic_scope_t::HCA,
          lci::net_atomic_scope_t::GLOBAL}) {
      lci::error_t fetch_error =
          lci::net_post_fetch_add_x(0, &result, lci::MR_HOST, 1, 0,
                                    lci::RMR_NULL)
              .runtime(runtime)
              .required_atomic_scope(required_scope)
              .device(device)
              .endpoint(endpoint)();
      lci::error_t add_error = lci::net_post_add_x(0, 1, 0, lci::RMR_NULL)
                                   .runtime(runtime)
                                   .required_atomic_scope(required_scope)
                                   .device(device)
                                   .endpoint(endpoint)();
      check(fetch_error.errorcode == lci::errorcode_t::fatal,
            "unsupported fetch-add scope was accepted");
      check(add_error.errorcode == lci::errorcode_t::fatal,
            "unsupported add scope was accepted");
    }
    lci::error_t default_fetch_error =
        lci::net_post_fetch_add_x(0, &result, lci::MR_HOST, 1, 0, lci::RMR_NULL)
            .runtime(runtime)
            .device(device)
            .endpoint(endpoint)();
    lci::error_t default_add_error = lci::net_post_add_x(0, 1, 0, lci::RMR_NULL)
                                         .runtime(runtime)
                                         .device(device)
                                         .endpoint(endpoint)();
    check(default_fetch_error.errorcode == lci::errorcode_t::fatal,
          "unsupported default fetch-add scope was accepted");
    check(default_add_error.errorcode == lci::errorcode_t::fatal,
          "unsupported default add scope was accepted");
    free_runtime_objects(runtime, &device, &endpoint);
    return 0;
  }

  if (net_context.get_attr_backend() != lci::attr_backend_t::ibv ||
      atomic_scope == lci::net_atomic_scope_t::NONE) {
    free_runtime_objects(runtime, &device, &endpoint);
    return kSkipReturnCode;
  }

#if !defined(LCI_USE_HIP)
  if (mode == test_mode_t::require_ibv_gpu) {
    free_runtime_objects(runtime, &device, &endpoint);
    return kSkipReturnCode;
  }
#endif  // LCI_USE_HIP

  check(lci::get_rank_n() == 2,
        "the IBV network atomic test requires exactly two ranks");
  const int rank = lci::get_rank_me();
  const int peer = 1 - rank;
  if (rank == 0) {
    std::fprintf(stderr, "IBV uint64 atomic scope: %s\n",
                 atomic_scope_str(atomic_scope));
  }

  remote_words_t target = {};
  target.counter = 100;
  alignas(uint64_t) uint64_t fetch_result = 0;
  alignas(uint64_t) uint64_t ordered_value = 0x123456789abcdef0ULL;
  std::array<unsigned char, 16> misaligned_result_storage = {};

  lci::mr_t target_mr = lci::register_memory_x(&target, sizeof(target))
                            .runtime(runtime)
                            .device(device)();
  lci::mr_t result_mr =
      lci::register_memory_x(&fetch_result, sizeof(fetch_result))
          .runtime(runtime)
          .device(device)();
  lci::mr_t ordered_value_mr =
      lci::register_memory_x(&ordered_value, sizeof(ordered_value))
          .runtime(runtime)
          .device(device)();
  lci::mr_t misaligned_result_mr =
      lci::register_memory_x(misaligned_result_storage.data(),
                             misaligned_result_storage.size())
          .runtime(runtime)
          .device(device)();

  lci::rmr_t local_rmr = lci::get_rmr(target_mr);
  std::array<lci::rmr_t, 2> local_rmrs = {local_rmr, local_rmr};
  std::array<lci::rmr_t, 2> remote_rmrs = {};
  lci::bootstrap::alltoall(local_rmrs.data(), remote_rmrs.data(),
                           sizeof(local_rmr));
  const lci::rmr_t peer_rmr = remote_rmrs[peer];
  constexpr uint64_t counter_offset = offsetof(remote_words_t, counter);

  bootstrap_barrier();
  if (rank == 0) {
    uint64_t* misaligned_result =
        reinterpret_cast<uint64_t*>(misaligned_result_storage.data() + 1);
    lci::error_t empty_rmr_fetch_error =
        lci::net_post_fetch_add_x(peer, &fetch_result, result_mr, 1,
                                  counter_offset, lci::RMR_NULL)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)();
    lci::error_t empty_rmr_add_error =
        lci::net_post_add_x(peer, 1, counter_offset, lci::RMR_NULL)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)();
    lci::error_t misaligned_result_error =
        lci::net_post_fetch_add_x(peer, misaligned_result, misaligned_result_mr,
                                  1, counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)();
    lci::error_t misaligned_remote_error =
        lci::net_post_fetch_add_x(peer, &fetch_result, result_mr, 1,
                                  counter_offset + 1, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)();
    check(empty_rmr_fetch_error.errorcode == lci::errorcode_t::fatal,
          "empty RMR fetch-add was accepted");
    check(empty_rmr_add_error.errorcode == lci::errorcode_t::fatal,
          "empty RMR add was accepted");
    check(misaligned_result_error.errorcode == lci::errorcode_t::fatal,
          "misaligned fetch-add result was accepted");
    check(misaligned_remote_error.errorcode == lci::errorcode_t::fatal,
          "misaligned fetch-add remote address was accepted");
  }
  bootstrap_barrier();

  if (rank == 0) {
    completion_context_t hca_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t hca_error =
        lci::net_post_add_x(peer, 0, counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(lci::net_atomic_scope_t::HCA)
            .device(device)
            .endpoint(endpoint)
            .user_context(&hca_context)();
    check(hca_error.is_posted(), "HCA atomic scope was not accepted");
    wait_for_completions(runtime, device, {&hca_context});

    completion_context_t global_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t global_error =
        lci::net_post_add_x(peer, 0, counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(lci::net_atomic_scope_t::GLOBAL)
            .device(device)
            .endpoint(endpoint)
            .user_context(&global_context)();
    if (atomic_scope == lci::net_atomic_scope_t::GLOBAL) {
      check(global_error.is_posted(),
            "GLOBAL atomic scope was not accepted by a GLOBAL backend");
      wait_for_completions(runtime, device, {&global_context});
    } else {
      check(global_error.errorcode == lci::errorcode_t::fatal,
            "HCA atomic capability was accepted as GLOBAL");
    }

    completion_context_t default_add_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t default_add_error =
        lci::net_post_add_x(peer, 0, counter_offset, peer_rmr)
            .runtime(runtime)
            .device(device)
            .endpoint(endpoint)
            .user_context(&default_add_context)();
    if (atomic_scope == lci::net_atomic_scope_t::GLOBAL) {
      check(default_add_error.is_posted(),
            "default add scope was not accepted by a GLOBAL backend");
      wait_for_completions(runtime, device, {&default_add_context});
    } else {
      check(default_add_error.errorcode == lci::errorcode_t::fatal,
            "default add scope accepted an HCA-only backend");
    }

    completion_context_t default_fetch_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t default_fetch_error =
        lci::net_post_fetch_add_x(peer, &fetch_result, result_mr, 0,
                                  counter_offset, peer_rmr)
            .runtime(runtime)
            .device(device)
            .endpoint(endpoint)
            .user_context(&default_fetch_context)();
    if (atomic_scope == lci::net_atomic_scope_t::GLOBAL) {
      check(default_fetch_error.is_posted(),
            "default fetch-add scope was not accepted by a GLOBAL backend");
      wait_for_completions(runtime, device, {&default_fetch_context});
      check(fetch_result == 100,
            "default fetch-add returned the wrong previous value");
    } else {
      check(default_fetch_error.errorcode == lci::errorcode_t::fatal,
            "default fetch-add scope accepted an HCA-only backend");
    }

    lci::error_t no_scope_fetch_error =
        lci::net_post_fetch_add_x(peer, &fetch_result, result_mr, 0,
                                  counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(lci::net_atomic_scope_t::NONE)
            .device(device)
            .endpoint(endpoint)();
    lci::error_t no_scope_add_error =
        lci::net_post_add_x(peer, 0, counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(lci::net_atomic_scope_t::NONE)
            .device(device)
            .endpoint(endpoint)();
    check(no_scope_fetch_error.errorcode == lci::errorcode_t::fatal,
          "fetch-add accepted a NONE required atomic scope");
    check(no_scope_add_error.errorcode == lci::errorcode_t::fatal,
          "add accepted a NONE required atomic scope");
  }
  bootstrap_barrier();

  if (rank == 0) {
    completion_context_t fetch_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t error =
        lci::net_post_fetch_add_x(peer, &fetch_result, result_mr, 7,
                                  counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)
            .user_context(&fetch_context)();
    check(error.is_posted(), "fetch-add was not posted");
    wait_for_completions(runtime, device, {&fetch_context});
    check(fetch_result == 100, "fetch-add returned the wrong previous value");
  }
  bootstrap_barrier();
  if (rank == 1) {
    check(target.counter == 107, "fetch-add did not update the remote value");
  }
  bootstrap_barrier();

  if (rank == 0) {
    completion_context_t add_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t error = lci::net_post_add_x(peer, 3, counter_offset, peer_rmr)
                             .runtime(runtime)
                             .required_atomic_scope(atomic_scope)
                             .device(device)
                             .endpoint(endpoint)
                             .user_context(&add_context)();
    check(error.is_posted(), "non-fetch add was not posted");
    wait_for_completions(runtime, device, {&add_context});
  }
  bootstrap_barrier();
  if (rank == 1) {
    check(target.counter == 110,
          "non-fetch add did not update the remote value");
  }
  bootstrap_barrier();

  if (rank == 0) {
    completion_context_t discarded_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t discarded_error =
        lci::net_post_add_x(peer, 4, counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)
            .user_context(&discarded_context)();
    check(discarded_error.is_posted(),
          "discarded non-fetch add was not posted");
    wait_for_discarded_completion(runtime, device);
    check(endpoint.get_impl()->get_pending_ops() == 0,
          "null-status poll did not retire the non-fetch add");
    lci::wait_drained_x().runtime(runtime).device(device)();

    completion_context_t recovered_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t recovered_error =
        lci::net_post_add_x(peer, 6, counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)
            .user_context(&recovered_context)();
    check(recovered_error.is_posted(),
          "non-fetch add did not reuse retired tracker storage");
    wait_for_completions(runtime, device, {&recovered_context});
    check(endpoint.get_impl()->get_pending_ops() == 0,
          "reused non-fetch add remained pending");
  }
  bootstrap_barrier();
  if (rank == 1) {
    check(target.counter == 120,
          "null-status poll did not retire the non-fetch add");
  }
  bootstrap_barrier();

  if (rank == 0) {
    completion_context_t write_context{lci::net_opcode_t::WRITE};
    completion_context_t ordered_fetch_context{lci::net_opcode_t::FETCH_ADD};
    lci::error_t put_error =
        lci::net_post_put_x(peer, &ordered_value, sizeof(ordered_value),
                            ordered_value_mr, counter_offset, peer_rmr)
            .runtime(runtime)
            .device(device)
            .endpoint(endpoint)
            .user_context(&write_context)();
    lci::error_t fetch_error =
        lci::net_post_fetch_add_x(peer, &fetch_result, result_mr, 5,
                                  counter_offset, peer_rmr)
            .runtime(runtime)
            .required_atomic_scope(atomic_scope)
            .device(device)
            .endpoint(endpoint)
            .user_context(&ordered_fetch_context)();
    check(put_error.is_posted(), "ordering write was not posted");
    check(fetch_error.is_posted(), "ordering fetch-add was not posted");
    wait_for_completions(runtime, device,
                         {&write_context, &ordered_fetch_context});
    check(fetch_result == ordered_value,
          "ordering fetch-add returned the wrong previous value");
  }
  bootstrap_barrier();
  if (rank == 1) {
    check(target.counter == ordered_value + 5,
          "ordering fetch-add did not update the remote value");
  }
  bootstrap_barrier();

  if (rank == 0) {
    std::array<completion_context_t, 5> add_contexts = {
        completion_context_t{lci::net_opcode_t::FETCH_ADD},
        completion_context_t{lci::net_opcode_t::FETCH_ADD},
        completion_context_t{lci::net_opcode_t::FETCH_ADD},
        completion_context_t{lci::net_opcode_t::FETCH_ADD},
        completion_context_t{lci::net_opcode_t::FETCH_ADD},
    };
    std::vector<completion_context_t*> first_batch;
    for (size_t i = 0; i < 4; ++i) {
      lci::error_t error = endpoint.get_impl()->post_add(
          peer, 1, counter_offset, peer_rmr, atomic_scope, &add_contexts[i],
          false /* allow_retry */);
      check(error.is_posted(), "batched non-fetch add was not posted");
      first_batch.push_back(&add_contexts[i]);
    }
    lci::error_t backlog_error = endpoint.get_impl()->post_add(
        peer, 1, counter_offset, peer_rmr, atomic_scope, &add_contexts.back(),
        false /* allow_retry */);
    check(backlog_error.errorcode == lci::errorcode_t::posted_backlog,
          "non-fetch add did not enter the backlog when the QP was full");
    check(!endpoint.get_impl()->is_backlog_queue_empty(peer),
          "non-fetch add backlog entry was lost");
    wait_for_completions(runtime, device, first_batch);
    check(endpoint.get_impl()->get_pending_ops() == 0,
          "completed non-fetch adds remained pending");
    check(endpoint.get_impl()->progress_backlog_queue(),
          "non-fetch add backlog entry did not progress");
    check(endpoint.get_impl()->is_backlog_queue_empty(peer),
          "non-fetch add backlog was not drained");
    wait_for_completions(runtime, device, {&add_contexts.back()});
    check(endpoint.get_impl()->get_pending_ops() == 0,
          "backlogged non-fetch add remained pending");
  }
  bootstrap_barrier();
  if (rank == 1) {
    check(target.counter == ordered_value + 10,
          "batched or backlogged non-fetch adds lost an update");
  }
  bootstrap_barrier();

#if defined(LCI_USE_HIP)
  if (mode == test_mode_t::require_ibv_gpu) {
    run_gpu_atomic_test(runtime, net_context, device, endpoint, peer,
                        atomic_scope);
  }
#endif  // LCI_USE_HIP

  lci::deregister_memory_x(&misaligned_result_mr).runtime(runtime)();
  lci::deregister_memory_x(&ordered_value_mr).runtime(runtime)();
  lci::deregister_memory_x(&result_mr).runtime(runtime)();
  lci::deregister_memory_x(&target_mr).runtime(runtime)();
  free_runtime_objects(runtime, &device, &endpoint);
  return 0;
}
}  // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr,
                 "Usage: %s --expect-unsupported|--require-ibv|"
                 "--require-ibv-gpu\n",
                 argv[0]);
    return 2;
  }
  if (std::strcmp(argv[1], "--expect-unsupported") == 0) {
    return run_test(test_mode_t::expect_unsupported);
  }
  if (std::strcmp(argv[1], "--require-ibv") == 0) {
    return run_test(test_mode_t::require_ibv);
  }
  if (std::strcmp(argv[1], "--require-ibv-gpu") == 0) {
    return run_test(test_mode_t::require_ibv_gpu);
  }
  std::fprintf(stderr, "Unknown mode '%s'\n", argv[1]);
  return 2;
}
