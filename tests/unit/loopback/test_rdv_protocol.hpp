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

// A rendezvous transfer whose RTS and RTR fit the inject size must not hold a
// packet while it is in flight. Otherwise, a process that posts many
// rendezvous sends drains the packet pool, the receives cannot be reposted,
// and the RTRs never land.
//
// Post many rendezvous AMs and sends to ourselves without waiting for any of
// them, through a device whose packet pool is only twice its posted receives.
// Check that no post takes a packet (sender) and that, whenever progress
// returns, every packet is either free or posted as a receive (receiver).
void exercise_many_inflight_rdv()
{
  constexpr lci::tag_t tag = 0xcafe;
  constexpr size_t nrecvs = 64;
  constexpr int nmsgs = 1000;  // per primitive
  const int rank = lci::get_rank_me();

  const size_t max_inject_size =
      lci::get_default_net_context().get_attr_max_inject_size();
  if (std::max(sizeof(lci::rts_msg_t), sizeof(lci::rtr_msg_t)) >
      max_inject_size) {
    GTEST_SKIP() << "The rendezvous messages do not fit the inject size "
                 << max_inject_size;
  }

  lci::packet_pool_t packet_pool =
      lci::alloc_packet_pool_x().npackets(2 * nrecvs)();
  lci::device_t device =
      lci::alloc_device_x().net_max_recvs(nrecvs).packet_pool(packet_pool)();
  const size_t msg_size =
      lci::get_max_bcopy_size_x().packet_pool(packet_pool)() + 1;
  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);

  void* send_buffer = malloc(msg_size);
  char* recv_buffers = static_cast<char*>(malloc(nmsgs * msg_size));
  util::write_buffer(send_buffer, msg_size, 's');
  util::write_buffer(recv_buffers, nmsgs * msg_size, 'r');

  // Pre-post the receives so the RTSs of the sends match on arrival.
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    KEEP_RETRY(status, lci::post_recv_x(rank, recv_buffers + i * msg_size,
                                        msg_size, tag, rcq)
                           .device(device)());
    ASSERT_TRUE(status.is_posted());
  }

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(60);
  int nposted = 0;
  int nposted_with_packet = 0;
  int nprogress_with_held_packet = 0;
  int nlocal_done = 0;
  int nremote_done = 0;
  while (nlocal_done < 2 * nmsgs || nremote_done < 2 * nmsgs) {
    ASSERT_LT(std::chrono::steady_clock::now(), deadline)
        << "rendezvous flood stuck: posted " << nposted << "/" << 2 * nmsgs
        << ", local completions " << nlocal_done << ", remote completions "
        << nremote_done;
    if (nposted < 2 * nmsgs) {
      const size_t npackets_before = packet_pool.get_impl()->get_size();
      lci::status_t status;
      if (nposted % 2 == 0) {
        status = lci::post_am_x(rank, send_buffer, msg_size, lcq, rcomp)
                     .device(device)();
      } else {
        status = lci::post_send_x(rank, send_buffer, msg_size, tag, lcq)
                     .device(device)();
      }
      if (!status.is_retry()) {
        ASSERT_TRUE(status.is_posted());
        if (packet_pool.get_impl()->get_size() != npackets_before) {
          nposted_with_packet++;
        }
        nposted++;
      }
    }
    lci::progress_x().device(device)();
    if (packet_pool.get_impl()->get_size() +
            device.get_impl()->get_nrecvs_posted() !=
        2 * nrecvs) {
      nprogress_with_held_packet++;
    }
    lci::status_t status = lci::cq_pop(lcq);
    if (status.is_done()) {
      nlocal_done++;
    }
    status = lci::cq_pop(rcq);
    if (status.is_done()) {
      ASSERT_EQ(status.size, msg_size);
      util::check_buffer(status.buffer, msg_size, 's');
      if (status.tag != tag) {
        ASSERT_EQ(status.tag, 0);  // an AM, whose buffer LCI allocated
        free(status.buffer);
      }
      nremote_done++;
    }
  }

  EXPECT_EQ(nposted_with_packet, 0) << "rendezvous sends took a packet";
  EXPECT_EQ(nprogress_with_held_packet, 0)
      << "the progress engine held packets for rendezvous messages";

  free(recv_buffers);
  free(send_buffer);
  lci::deregister_rcomp(rcomp);
  lci::free_comp(&rcq);
  lci::free_comp(&lcq);
  lci::free_device(&device);
  lci::free_packet_pool(&packet_pool);
}

// In writeimm mode, every accepted RTS takes an immediate data tag until its
// data arrives. When all tags are taken, the receiver must complete the
// message with the write protocol instead.
void exercise_exhausted_imm_archive()
{
  auto& archive = lci::get_g_runtime().get_impl()->rdv_imm_archive;
  std::vector<lci::imm_tag_archive_t::tag_t> tags;
  lci::imm_tag_archive_t::tag_t imm_tag;
  while (archive.try_insert(0, &imm_tag)) {
    tags.push_back(imm_tag);
  }
  ASSERT_EQ(tags.size(), archive.capacity());

  exercise_large_sendrecv();
  // The message neither took nor leaked a tag.
  ASSERT_FALSE(archive.try_insert(0, &imm_tag));

  for (auto tag : tags) {
    archive.remove(tag);
  }
  exercise_large_sendrecv();
}

void run_protocol_case(lci::attr_rdv_protocol_t protocol,
                       bool requires_putimm_support,
                       void (*exercise)() = exercise_large_sendrecv)
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

  exercise();

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

TEST(RDV_PROTOCOL, WriteImmArchiveExhausted)
{
  run_protocol_case(lci::attr_rdv_protocol_t::writeimm, true,
                    exercise_exhausted_imm_archive);
}

TEST(RDV_PROTOCOL, ManyInflightWrite)
{
  run_protocol_case(lci::attr_rdv_protocol_t::write, false,
                    exercise_many_inflight_rdv);
}

TEST(RDV_PROTOCOL, ManyInflightWriteImm)
{
  run_protocol_case(lci::attr_rdv_protocol_t::writeimm, true,
                    exercise_many_inflight_rdv);
}

}  // namespace test_rdv_protocol
