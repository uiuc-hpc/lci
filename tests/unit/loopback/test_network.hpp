// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

namespace test_network
{
TEST(NETWORK, completion_batch_processes_successes_and_all_errors)
{
  lci::g_runtime_init();
  lci::runtime_t runtime = lci::g_default_runtime;
  lci::device_t device = lci::get_default_device();
  lci::endpoint_t endpoint = lci::get_default_endpoint();
  const int64_t initial_pending = endpoint.get_impl()->get_pending_ops();
  lci::comp_t successful = lci::alloc_counter();
  lci::comp_t failed = lci::alloc_counter();

  auto make_context = [&](int rank, lci::comp_t completion) {
    auto* context = new lci::internal_context_t;
    context->set_user_posted_op(endpoint);
    context->rank = rank;
    context->comp = completion;
    return context;
  };
  auto make_status = [](lci::net_opcode_t opcode, int rank, void* context) {
    lci::net_status_t status = {};
    status.opcode = opcode;
    status.rank = rank;
    status.user_context = context;
    return status;
  };

  lci::net_status_t statuses[] = {
      make_status(lci::net_opcode_t::SEND, 3, make_context(3, successful)),
      make_status(lci::net_opcode_t::ERROR, 7, make_context(7, failed)),
      make_status(lci::net_opcode_t::SEND, 4, make_context(4, successful)),
      make_status(lci::net_opcode_t::ERROR, 8, make_context(8, failed)),
  };

  try {
    lci::process_completion_batch(runtime, device, endpoint, statuses,
                                  sizeof(statuses) / sizeof(statuses[0]));
    FAIL() << "Expected peer_failure_error";
  } catch (const lci::peer_failure_error& error) {
    EXPECT_EQ(error.failed_rank(), 7);
  }

  EXPECT_EQ(lci::counter_get(successful), 2);
  EXPECT_EQ(lci::counter_get(failed), 0);
  EXPECT_EQ(endpoint.get_impl()->get_pending_ops(), initial_pending);

  lci::free_comp(&successful);
  lci::free_comp(&failed);
  lci::g_runtime_fina();
}

TEST(NETWORK, completion_batch_first_generic_failure_wins)
{
  lci::g_runtime_init();
  lci::runtime_t runtime = lci::g_default_runtime;
  lci::device_t device = lci::get_default_device();
  lci::endpoint_t endpoint = lci::get_default_endpoint();
  const int64_t initial_pending = endpoint.get_impl()->get_pending_ops();
  lci::comp_t failed = lci::alloc_counter();

  auto make_context = [&](int rank) {
    auto* context = new lci::internal_context_t;
    context->set_user_posted_op(endpoint);
    context->rank = rank;
    context->comp = failed;
    return context;
  };
  auto make_error = [](int rank, void* context) {
    lci::net_status_t status = {};
    status.opcode = lci::net_opcode_t::ERROR;
    status.rank = rank;
    status.user_context = context;
    return status;
  };

  lci::net_status_t statuses[] = {
      make_error(8, make_context(7)),
      make_error(9, make_context(9)),
  };

  try {
    lci::process_completion_batch(runtime, device, endpoint, statuses,
                                  sizeof(statuses) / sizeof(statuses[0]));
    FAIL() << "Expected generic runtime_error";
  } catch (const lci::peer_failure_error&) {
    FAIL() << "The first generic failure must control the exception";
  } catch (const std::runtime_error&) {
  }

  EXPECT_EQ(lci::counter_get(failed), 0);
  EXPECT_EQ(endpoint.get_impl()->get_pending_ops(), initial_pending);

  lci::free_comp(&failed);
  lci::g_runtime_fina();
}

TEST(NETWORK, completion_batch_reports_rank_without_completion_object)
{
  lci::g_runtime_init();
  lci::runtime_t runtime = lci::g_default_runtime;
  lci::device_t device = lci::get_default_device();
  lci::endpoint_t endpoint = lci::get_default_endpoint();
  const int64_t initial_pending = endpoint.get_impl()->get_pending_ops();

  for (int backend_rank : {7, -1}) {
    auto* context = new lci::internal_context_t;
    context->set_user_posted_op(endpoint);
    context->rank = 7;
    EXPECT_TRUE(context->comp.is_empty());

    lci::net_status_t status = {};
    status.opcode = lci::net_opcode_t::ERROR;
    status.rank = backend_rank;
    status.user_context = context;

    try {
      lci::process_completion_batch(runtime, device, endpoint, &status, 1);
      FAIL() << "Expected peer_failure_error";
    } catch (const lci::peer_failure_error& error) {
      EXPECT_EQ(error.failed_rank(), 7);
    }

    EXPECT_EQ(endpoint.get_impl()->get_pending_ops(), initial_pending);
  }

  lci::g_runtime_fina();
}

TEST(NETWORK, completion_batch_unknown_rank_is_generic)
{
  lci::g_runtime_init();
  lci::runtime_t runtime = lci::g_default_runtime;
  lci::device_t device = lci::get_default_device();
  lci::endpoint_t endpoint = lci::get_default_endpoint();

  lci::net_status_t status = {};
  status.opcode = lci::net_opcode_t::ERROR;
  status.rank = -1;
  status.user_context = nullptr;

  try {
    lci::process_completion_batch(runtime, device, endpoint, &status, 1);
    FAIL() << "Expected generic runtime_error";
  } catch (const lci::peer_failure_error&) {
    FAIL() << "An unknown rank must not report peer_failure_error";
  } catch (const std::runtime_error&) {
  }

  lci::g_runtime_fina();
}

TEST(NETWORK, reg_mem)
{
  lci::g_runtime_init();
  const int size = 1024;
  void* address = malloc(size);
  lci::mr_t mr = lci::register_memory(address, size);
  lci::deregister_memory(&mr);
  lci::g_runtime_fina();
}

// Regression coverage for zero-sized registrations (bypasses UCX cache path).
TEST(NETWORK, reg_mem_zero_size)
{
  lci::g_runtime_init();
  void* address = malloc(1);
  lci::mr_t mr = lci::register_memory(address, 0);
  ASSERT_NE(mr.p_impl, nullptr);
  lci::rmr_t rmr = lci::get_rmr(mr);
  lci::deregister_memory(&mr);
  free(address);
  lci::g_runtime_fina();
}

TEST(NETWORK, poll_cq)
{
  lci::g_runtime_init();
  lci::net_status_t statuses[LCI_BACKEND_MAX_POLLS];
  size_t ret = lci::net_poll_cq(LCI_BACKEND_MAX_POLLS, statuses);
  ASSERT_EQ(ret, 0);
  lci::g_runtime_fina();
}

TEST(NETWORK, loopback)
{
  // Raw network receives must not compete with LCI's packet-pool receives.
  lci::g_runtime_init_x().alloc_default_packet_pool(false)();
  const int size = 1024;
  void* address = malloc(size);
  memset(address, 0, size);
  lci::mr_t mr = lci::register_memory(address, size);
  int recv_context = 0;
  int send_context = 0;
  while (lci::net_post_recv_x(address, size, mr)
             .user_context(&recv_context)()
             .is_retry())
    continue;
  while (lci::net_post_send_x(0, address, size, mr)
             .user_context(&send_context)()
             .is_retry())
    continue;
  bool received_recv = false;
  bool received_send = false;
  while (!received_recv || !received_send) {
    lci::net_status_t statuses[LCI_BACKEND_MAX_POLLS];
    size_t ret = lci::net_poll_cq(LCI_BACKEND_MAX_POLLS, statuses);
    for (size_t i = 0; i < ret; ++i) {
      if (statuses[i].opcode == lci::net_opcode_t::RECV) {
        EXPECT_FALSE(received_recv);
        EXPECT_EQ(statuses[i].user_context, &recv_context);
        received_recv = true;
      } else if (statuses[i].opcode == lci::net_opcode_t::SEND) {
        EXPECT_FALSE(received_send);
        EXPECT_EQ(statuses[i].user_context, &send_context);
        received_send = true;
      } else {
        FAIL() << "Unexpected raw loopback completion opcode "
               << lci::get_net_opcode_str(statuses[i].opcode);
      }
    }
  }
  lci::deregister_memory(&mr);
  lci::g_runtime_fina();
}

void test_loopback_mt(int id, int nmsgs, int size, void* address, lci::mr_t mr)
{
  for (int i = 0; i < nmsgs; ++i) {
    std::atomic<int> count(0);
    while (lci::net_post_recv_x(address, size, mr)
               .user_context(&count)()
               .is_retry())
      continue;
    while (lci::net_post_send_x(0, address, size, mr)
               .user_context(&count)()
               .is_retry())
      continue;
    while (count.load() < 2) {
      lci::net_status_t status;
      size_t ret = lci::net_poll_cq(1, &status);
      if (ret > 0) {
        ASSERT_EQ(ret, 1);
        auto* p = (std::atomic<int>*)status.user_context;
        p->fetch_add(1);
      }
    }
    ASSERT_EQ(count.load(), 2);
  }
}

TEST(NETWORK, loopback_mt)
{
  lci::g_runtime_init_x().alloc_default_packet_pool(false)();
  const int size = 1024;
  const int nthreads = util::NTHREADS;
  const int nmsgs = util::NITERS;
  ASSERT_EQ(nmsgs % nthreads, 0);
  void* address = malloc(size);
  memset(address, 0, size);
  lci::mr_t mr = lci::register_memory(address, size);
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_loopback_mt, i, nmsgs / nthreads, size, address, mr);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  lci::deregister_memory(&mr);
  lci::g_runtime_fina();
}

TEST(NETWORK, loopback_put)
{
  lci::g_runtime_init();
  const int size = 1024;
  void* send_address = malloc(size);
  void* recv_address = malloc(size);
  memset(send_address, 0, size);
  memset(recv_address, 0, size);
  lci::mr_t send_mr = lci::register_memory(send_address, size);
  lci::mr_t recv_mr = lci::register_memory(recv_address, size);
  lci::rmr_t rmr = lci::get_rmr(recv_mr);
  while (lci::net_post_put(0, send_address, size, send_mr, 0, rmr).is_retry())
    continue;
  size_t total = 0;
  while (total < 1) {
    lci::net_status_t status;
    size_t ret = lci::net_poll_cq(1, &status);
    total += ret;
  }
  lci::deregister_memory(&send_mr);
  lci::deregister_memory(&recv_mr);
  lci::g_runtime_fina();
}

TEST(NETWORK, loopback_get)
{
  lci::g_runtime_init();
  const int size = 1024;
  void* send_address = malloc(size);
  void* recv_address = malloc(size);
  memset(send_address, 0, size);
  memset(recv_address, 0, size);
  lci::mr_t send_mr = lci::register_memory(send_address, size);
  lci::mr_t recv_mr = lci::register_memory(recv_address, size);
  lci::rmr_t rmr = lci::get_rmr(send_mr);
  while (lci::net_post_get(0, recv_address, size, recv_mr, 0, rmr).is_retry())
    continue;
  size_t total = 0;
  while (total < 1) {
    lci::net_status_t status;
    size_t ret = lci::net_poll_cq(1, &status);
    total += ret;
  }
  lci::deregister_memory(&send_mr);
  lci::deregister_memory(&recv_mr);
  lci::g_runtime_fina();
}

}  // namespace test_network
