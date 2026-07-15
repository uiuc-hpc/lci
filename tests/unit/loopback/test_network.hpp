// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

namespace test_network
{
TEST(NETWORK, completion_error_metadata)
{
  int context = 0;
  lci::network_completion_error error(
      "completion failed", lci::option_t<int>(7),
      lci::option_t<void*>(static_cast<void*>(&context)));

  int rank = -1;
  void* user_context = nullptr;
  EXPECT_TRUE(error.failed_rank().get_set_value(&rank));
  EXPECT_EQ(rank, 7);
  EXPECT_TRUE(error.user_context().get_set_value(&user_context));
  EXPECT_EQ(user_context, &context);
  EXPECT_FALSE(error.has_lci_outgoing_context());

  lci::network_completion_error empty_error("completion failed");
  EXPECT_FALSE(empty_error.failed_rank().get_set_value(&rank));
  EXPECT_FALSE(empty_error.user_context().get_set_value(&user_context));
  EXPECT_FALSE(empty_error.has_lci_outgoing_context());
}

TEST(NETWORK, ordered_completion_events)
{
  lci::ordered_completion_event_queue_t events;
  lci::net_status_t first = {};
  first.user_context = reinterpret_cast<void*>(1);
  lci::net_status_t last = {};
  last.user_context = reinterpret_cast<void*>(3);

  events.push_success(first);
  events.push_error("completion failed", lci::option_t<int>(7),
                    lci::option_t<void*>(reinterpret_cast<void*>(2)));
  events.push_success(last);

  lci::net_status_t status = {};
  EXPECT_EQ(events.drain(&status, 4), 1);
  EXPECT_EQ(status.user_context, first.user_context);

  try {
    (void)events.drain(&status, 4);
    FAIL() << "Expected the ordered error event";
  } catch (const lci::network_completion_error& error) {
    int rank = -1;
    void* user_context = nullptr;
    EXPECT_TRUE(error.failed_rank().get_set_value(&rank));
    EXPECT_EQ(rank, 7);
    EXPECT_TRUE(error.user_context().get_set_value(&user_context));
    EXPECT_EQ(user_context, reinterpret_cast<void*>(2));
  }

  EXPECT_EQ(events.drain(&status, 4), 1);
  EXPECT_EQ(status.user_context, last.user_context);
  EXPECT_TRUE(events.empty());
}

TEST(NETWORK, internal_context_classification)
{
  lci::internal_context_t simple(lci::internal_context_kind_t::simple_outgoing);
  lci::internal_context_t receive(lci::internal_context_kind_t::posted_receive);
  lci::internal_context_t rendezvous(
      lci::internal_context_kind_t::rendezvous_root);
  lci::internal_context_t rtr(lci::internal_context_kind_t::rtr_control);
  lci::internal_context_extended_t split(
      lci::internal_context_kind_t::split_transfer);
  lci::internal_context_extended_t fallback(
      lci::internal_context_kind_t::putimm_fallback);

  EXPECT_EQ(simple.kind, lci::internal_context_kind_t::simple_outgoing);
  EXPECT_EQ(receive.kind, lci::internal_context_kind_t::posted_receive);
  EXPECT_EQ(rendezvous.kind, lci::internal_context_kind_t::rendezvous_root);
  EXPECT_EQ(rtr.kind, lci::internal_context_kind_t::rtr_control);
  EXPECT_TRUE(lci::is_extended_context_kind(split.kind));
  EXPECT_TRUE(lci::is_extended_context_kind(fallback.kind));
  EXPECT_FALSE(lci::is_extended_context_kind(simple.kind));
}

TEST(NETWORK, completion_error_translation_is_bounded)
{
  lci::g_runtime_init();
  lci::endpoint_t endpoint = lci::get_default_endpoint();
  const int64_t initial_pending = endpoint.get_impl()->get_pending_ops();
  lci::comp_t completion = lci::alloc_cq();

  auto* simple = new lci::internal_context_t(
      lci::internal_context_kind_t::simple_outgoing);
  simple->set_user_posted_op(endpoint);
  simple->rank = 7;
  simple->comp = completion;
  try {
    lci::translate_network_completion_error(lci::network_completion_error(
        "completion failed", lci::option_t<int>(7),
        lci::option_t<void*>(static_cast<void*>(simple)), true));
    FAIL() << "Expected peer_failure_error";
  } catch (const lci::peer_failure_error& error) {
    EXPECT_EQ(error.failed_rank(), 7);
  }
  EXPECT_EQ(endpoint.get_impl()->get_pending_ops(), initial_pending);
  EXPECT_TRUE(lci::cq_pop(completion).is_retry());

  auto* mismatch = new lci::internal_context_t(
      lci::internal_context_kind_t::simple_outgoing);
  mismatch->set_user_posted_op(endpoint);
  mismatch->rank = 7;
  try {
    lci::translate_network_completion_error(lci::network_completion_error(
        "completion failed", lci::option_t<int>(8),
        lci::option_t<void*>(static_cast<void*>(mismatch)), true));
    FAIL() << "Expected generic runtime_error";
  } catch (const lci::peer_failure_error&) {
    FAIL() << "Rank disagreement must not produce a typed peer failure";
  } catch (const std::runtime_error&) {
  }
  EXPECT_EQ(endpoint.get_impl()->get_pending_ops(), initial_pending);

  auto* unsupported =
      new lci::internal_context_t(lci::internal_context_kind_t::rtr_control);
  try {
    lci::translate_network_completion_error(lci::network_completion_error(
        "completion failed", lci::option_t<int>(),
        lci::option_t<void*>(static_cast<void*>(unsupported)), true));
    FAIL() << "Expected generic runtime_error";
  } catch (const lci::peer_failure_error&) {
    FAIL() << "Control contexts must not produce typed recovery";
  } catch (const std::runtime_error&) {
  }
  delete unsupported;

  try {
    lci::translate_network_completion_error(lci::network_completion_error(
        "completion failed", lci::option_t<int>(7)));
    FAIL() << "Expected generic runtime_error";
  } catch (const lci::peer_failure_error&) {
    FAIL() << "A backend rank without a context must remain generic";
  } catch (const std::runtime_error&) {
  }

  lci::free_comp(&completion);
  lci::g_runtime_fina();
}

TEST(NETWORK, receive_completion_error_context_stays_generic)
{
  void* receive_context = reinterpret_cast<void*>(1);
  lci::network_completion_error error("receive completion failed",
                                      lci::option_t<int>(),
                                      lci::option_t<void*>(receive_context));

  try {
    lci::translate_network_completion_error(error);
    FAIL() << "Expected generic runtime_error";
  } catch (const lci::peer_failure_error&) {
    FAIL() << "Receive contexts must not produce typed recovery";
  } catch (const std::runtime_error&) {
  }

  void* preserved_context = nullptr;
  EXPECT_TRUE(error.user_context().get_set_value(&preserved_context));
  EXPECT_EQ(preserved_context, receive_context);
  EXPECT_FALSE(error.has_lci_outgoing_context());
}

TEST(NETWORK, packet_receive_context_ownership)
{
  lci::g_runtime_init();
  auto* device = lci::get_default_device().get_impl();
  int raw_context = 0;
  const size_t nrecvs_posted = device->get_nrecvs_posted();
  EXPECT_FALSE(device->is_packet_recv_context(&raw_context));
  device->consume_packet_recv(&raw_context);
  EXPECT_EQ(device->get_nrecvs_posted(), nrecvs_posted);

  lci::packet_t* packet = device->packet_pool.get_impl()->get();
  ASSERT_NE(packet, nullptr);
  EXPECT_TRUE(device->is_packet_recv_context(packet));
  packet->put_back();
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
  while (lci::net_post_recv(address, size, mr).is_retry()) continue;
  while (lci::net_post_send(0, address, size, mr).is_retry()) continue;
  size_t total = 0;
  while (total < 2) {
    lci::net_status_t statuses[LCI_BACKEND_MAX_POLLS];
    size_t ret = lci::net_poll_cq(LCI_BACKEND_MAX_POLLS, statuses);
    total += ret;
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
