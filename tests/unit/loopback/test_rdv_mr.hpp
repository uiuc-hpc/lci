// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

// The rendezvous protocol must use the memory region the user supplied
// instead of registering the buffer on the fly, and must never deregister it.
namespace test_rdv_mr
{
namespace
{
uint64_t get_nmrs_registered()
{
  return lci::get_g_runtime()
      .get_impl()
      ->default_device.get_impl()
      ->get_nmrs_registered();
}

lci::status_t wait_cq(lci::comp_t cq)
{
  lci::status_t status;
  do {
    lci::progress();
    status = lci::cq_pop(cq);
  } while (status.is_retry());
  return status;
}

// A registered region with the message buffer strictly inside it, so the mr
// covers more than [buffer, buffer + size).
struct registered_buffer_t {
  static constexpr size_t padding = 4096;
  char* region;
  size_t size;
  lci::mr_t mr;

  explicit registered_buffer_t(size_t size_) : size(size_)
  {
    region = static_cast<char*>(malloc(size + 2 * padding));
    mr = lci::register_memory(region, size + 2 * padding);
  }
  ~registered_buffer_t()
  {
    lci::deregister_memory(&mr);
    free(region);
  }
  void* buffer() const { return region + padding; }
};

// Send one rendezvous message from send_buffer to recv_buffer and check it.
// If !recv_first, the RTS is likely to arrive before the receive is posted.
void sendrecv(void* send_buffer, lci::mr_t send_mr, void* recv_buffer,
              lci::mr_t recv_mr, size_t size, bool recv_first)
{
  const int rank = lci::get_rank_me();
  constexpr lci::tag_t tag = 0x2a;
  lci::comp_t send_cq = lci::alloc_cq();
  lci::comp_t recv_cq = lci::alloc_cq();
  util::write_buffer(send_buffer, size, 's');
  util::write_buffer(recv_buffer, size, 'r');

  lci::status_t status;
  auto post_recv = [&]() {
    KEEP_RETRY(
        status,
        lci::post_recv_x(rank, recv_buffer, size, tag, recv_cq).mr(recv_mr)());
    ASSERT_TRUE(status.is_posted());
  };
  if (recv_first) post_recv();
  KEEP_RETRY(
      status,
      lci::post_send_x(rank, send_buffer, size, tag, send_cq).mr(send_mr)());
  ASSERT_TRUE(status.is_posted());
  if (!recv_first) {
    for (int i = 0; i < 100; ++i) lci::progress();
    post_recv();
  }
  EXPECT_TRUE(wait_cq(send_cq).is_done());
  status = wait_cq(recv_cq);
  ASSERT_TRUE(status.is_done());
  ASSERT_EQ(status.size, size);
  util::check_buffer(recv_buffer, size, 's');

  lci::free_comp(&recv_cq);
  lci::free_comp(&send_cq);
}

// Send one rendezvous active message and return the received status.
lci::status_t am(void* send_buffer, lci::mr_t send_mr, size_t size)
{
  const int rank = lci::get_rank_me();
  lci::comp_t send_cq = lci::alloc_cq();
  lci::comp_t recv_cq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(recv_cq);
  util::write_buffer(send_buffer, size, 'a');

  lci::status_t status;
  KEEP_RETRY(
      status,
      lci::post_am_x(rank, send_buffer, size, send_cq, rcomp).mr(send_mr)());
  EXPECT_TRUE(status.is_posted());
  EXPECT_TRUE(wait_cq(send_cq).is_done());
  status = wait_cq(recv_cq);
  EXPECT_TRUE(status.is_done());
  EXPECT_EQ(status.size, size);
  util::check_buffer(status.buffer, size, 'a');

  lci::deregister_rcomp(rcomp);
  lci::free_comp(&recv_cq);
  lci::free_comp(&send_cq);
  return status;
}

// Hands out rendezvous buffers from one pre-registered region, with its mr.
struct registered_allocator_t : public lci::allocator_base_t {
  registered_buffer_t pool;
  size_t next = 0;
  int nallocs_with_mr = 0;

  explicit registered_allocator_t(size_t size) : pool(size) {}
  void* allocate(size_t size) override { return malloc(size); }
  void* allocate_with_mr(size_t size, lci::device_t device,
                         lci::mr_t* mr) override
  {
    EXPECT_EQ(device, lci::get_g_runtime().get_impl()->default_device);
    if (next + size > pool.size) {
      *mr = lci::MR_HOST;
      return allocate(size);
    }
    void* ret = static_cast<char*>(pool.buffer()) + next;
    next += size;
    ++nallocs_with_mr;
    *mr = pool.mr;
    return ret;
  }
  void deallocate(void* ptr) override
  {
    if (!owns(ptr)) free(ptr);
  }
  bool owns(void* ptr) const
  {
    return ptr >= pool.buffer() &&
           ptr < static_cast<char*>(pool.buffer()) + pool.size;
  }
};

// Implements only the required methods.
struct plain_allocator_t : public lci::allocator_base_t {
  int nallocs = 0;
  void* allocate(size_t size) override
  {
    ++nallocs;
    return malloc(size);
  }
  void deallocate(void* ptr) override { free(ptr); }
};
}  // namespace

TEST(RDV_MR, sendrecv_uses_user_mr)
{
  lci::g_runtime_init();
  const size_t size = lci::get_max_bcopy_size() + 1024;
  {
    registered_buffer_t send_buf(size), recv_buf(size);
    for (bool recv_first : {true, false}) {
      // Twice with the same mrs: they must survive the first message.
      for (int i = 0; i < 2; ++i) {
        uint64_t nmrs = get_nmrs_registered();
        sendrecv(send_buf.buffer(), send_buf.mr, recv_buf.buffer(), recv_buf.mr,
                 size, recv_first);
        EXPECT_EQ(get_nmrs_registered() - nmrs, 0u);
      }
      // Without an mr, both sides register on the fly.
      uint64_t nmrs = get_nmrs_registered();
      sendrecv(send_buf.buffer(), lci::MR_HOST, recv_buf.buffer(), lci::MR_HOST,
               size, recv_first);
      EXPECT_EQ(get_nmrs_registered() - nmrs, 2u);
    }
  }
  lci::g_runtime_fina();
}

TEST(RDV_MR, am_send_uses_user_mr)
{
  lci::g_runtime_init();
  const size_t size = lci::get_max_bcopy_size() + 1024;
  {
    registered_buffer_t send_buf(size);
    for (int i = 0; i < 2; ++i) {
      uint64_t nmrs = get_nmrs_registered();
      lci::status_t status = am(send_buf.buffer(), send_buf.mr, size);
      // Only the receive buffer from the default allocator is registered.
      EXPECT_EQ(get_nmrs_registered() - nmrs, 1u);
      free(status.buffer);
    }
  }
  lci::g_runtime_fina();
}

TEST(RDV_MR, am_recv_uses_allocator_mr)
{
  lci::g_runtime_init();
  const size_t size = lci::get_max_bcopy_size() + 1024;
  {
    registered_buffer_t send_buf(size);
    // Room for two messages; the second starts inside the region.
    registered_allocator_t allocator(2 * size + 64);
    allocator.next = 64;
    lci::set_allocator(&allocator);
    for (int i = 0; i < 2; ++i) {
      uint64_t nmrs = get_nmrs_registered();
      lci::status_t status = am(send_buf.buffer(), send_buf.mr, size);
      EXPECT_EQ(get_nmrs_registered() - nmrs, 0u);
      EXPECT_TRUE(allocator.owns(status.buffer));
      EXPECT_EQ(allocator.nallocs_with_mr, i + 1);
      allocator.deallocate(status.buffer);
    }
    lci::set_allocator(&lci::g_allocator_default);
  }
  lci::g_runtime_fina();
}

TEST(RDV_MR, am_recv_plain_allocator)
{
  lci::g_runtime_init();
  const size_t size = lci::get_max_bcopy_size() + 1024;
  {
    registered_buffer_t send_buf(size);
    plain_allocator_t allocator;
    lci::set_allocator(&allocator);
    uint64_t nmrs = get_nmrs_registered();
    lci::status_t status = am(send_buf.buffer(), send_buf.mr, size);
    // The receive buffer is registered on the fly.
    EXPECT_EQ(get_nmrs_registered() - nmrs, 1u);
    EXPECT_EQ(allocator.nallocs, 1);
    allocator.deallocate(status.buffer);
    lci::set_allocator(&lci::g_allocator_default);
  }
  lci::g_runtime_fina();
}

}  // namespace test_rdv_mr
