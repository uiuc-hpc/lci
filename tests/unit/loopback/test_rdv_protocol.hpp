// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

namespace test_rdv_protocol
{
namespace
{
void exercise_large_sendrecv()
{
  constexpr lci::tag_t tag = 0xbeef;
  const int rank = lci::get_rank_me();
  const size_t msg_size = lci::get_max_bcopy_size() + 1024;

  lci::comp_t send_cq = lci::alloc_cq();
  lci::comp_t recv_cq = lci::alloc_cq();

  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  ASSERT_NE(send_buffer, nullptr);
  ASSERT_NE(recv_buffer, nullptr);

  util::write_buffer(send_buffer, msg_size, 's');
  util::write_buffer(recv_buffer, msg_size, 'r');

  lci::status_t status;
  bool poll_recv = false;
  KEEP_RETRY(status,
             lci::post_recv_x(rank, recv_buffer, msg_size, tag, recv_cq)());
  if (status.is_posted()) {
    poll_recv = true;
  } else {
    ASSERT_TRUE(status.is_done());
    ASSERT_EQ(status.size, msg_size);
  }

  bool poll_send = false;
  KEEP_RETRY(status,
             lci::post_send_x(rank, send_buffer, msg_size, tag, send_cq)());
  if (status.is_posted()) {
    poll_send = true;
  } else {
    ASSERT_TRUE(status.is_done());
  }

  if (poll_send) {
    do {
      status = lci::cq_pop(send_cq);
      if (status.is_retry()) {
        lci::progress();
      }
    } while (status.is_retry());
    ASSERT_TRUE(status.is_done());
  }

  if (poll_recv) {
    do {
      status = lci::cq_pop(recv_cq);
      if (status.is_retry()) {
        lci::progress();
      }
    } while (status.is_retry());
    ASSERT_TRUE(status.is_done());
    ASSERT_EQ(status.size, msg_size);
  }

  util::check_buffer(recv_buffer, msg_size, 's');

  free(recv_buffer);
  free(send_buffer);
  lci::free_comp(&recv_cq);
  lci::free_comp(&send_cq);
}

void run_protocol_case(lci::attr_rdv_protocol_t protocol,
                       bool requires_putimm_support)
{
  auto runtime = lci::g_runtime_init_x().rdv_protocol(protocol)();
  (void)runtime;

  auto actual_protocol = lci::get_g_runtime().get_attr_rdv_protocol();
  ASSERT_EQ(actual_protocol, protocol);

  if (requires_putimm_support) {
    auto net_ctx = lci::get_default_net_context();
    if (!net_ctx.is_empty() && !net_ctx.get_attr_support_putimm()) {
      lci::g_runtime_fina();
      GTEST_SKIP() << "Skipping writeimm rendezvous test: put-with-immediate "
                      "not supported by the backend.";
    }
  }

  exercise_large_sendrecv();

  lci::g_runtime_fina();
}
}  // namespace

TEST(RDV_PROTOCOL, Write)
{
  run_protocol_case(lci::attr_rdv_protocol_t::write, false);
}

TEST(RDV_PROTOCOL, WriteImm)
{
  run_protocol_case(lci::attr_rdv_protocol_t::writeimm, true);
}

}  // namespace test_rdv_protocol
