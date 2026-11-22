// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <chrono>
#include <cstdlib>

// The rendezvous protocol must use the memory region the user supplied
// instead of registering the buffer on the fly, and must never deregister it.
namespace test_rdv_mr
{
namespace
{
lci::device_t default_device()
{
  return lci::get_g_runtime().get_impl()->default_device;
}

// A second device shares the default packet pool and preposts receives from
// it. It only receives a few control messages here, so keep its quota small;
// a small LCI_ATTR_NPACKETS then still leaves packets for the rendezvous.
lci::device_t alloc_second_device()
{
  return lci::alloc_device_x().net_max_recvs(8)();
}

// The helpers below take an optional second device (`other`) to count
// registrations on and make progress on, besides the default device.
uint64_t get_nmrs_registered(lci::device_t other = lci::device_t())
{
  uint64_t n = default_device().get_impl()->get_nmrs_registered();
  if (!other.is_empty()) n += other.get_impl()->get_nmrs_registered();
  return n;
}

void progress(lci::device_t other)
{
  lci::progress();
  if (!other.is_empty()) lci::progress_x().device(other)();
}

// Abort instead of hanging until the ctest timeout if a message is lost.
template <typename Pred>
void wait_until(Pred pred, lci::device_t other)
{
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) {
      ADD_FAILURE() << "Timed out waiting for a rendezvous to complete";
      std::abort();
    }
    progress(other);
  }
}

lci::status_t wait_cq(lci::comp_t cq, lci::device_t other)
{
  lci::status_t status;
  wait_until([&]() { return !(status = lci::cq_pop(cq)).is_retry(); }, other);
  return status;
}

template <typename Post>
lci::status_t post_until_not_retry(Post post, lci::device_t other)
{
  lci::status_t status;
  wait_until([&]() { return !(status = post()).is_retry(); }, other);
  return status;
}

// A registered region with the message buffer strictly inside it, so the mr
// covers more than [buffer, buffer + size).
struct registered_buffer_t {
  static constexpr size_t padding = 4096;
  char* region;
  size_t size;
  lci::mr_t mr;

  explicit registered_buffer_t(size_t size_,
                               lci::device_t device = default_device())
      : size(size_)
  {
    region = static_cast<char*>(malloc(size + 2 * padding));
    mr = lci::register_memory_x(region, size + 2 * padding).device(device)();
  }
  ~registered_buffer_t()
  {
    lci::deregister_memory(&mr);
    free(region);
  }
  void* buffer() const { return region + padding; }
};

// Counts sends queued without a match, so a test can wait until an RTS is
// queued before it posts the receive.
struct counting_engine_t : public lci::matching_engine_map_t {
  int nsends_queued = 0;
  counting_engine_t() : matching_engine_map_t(attr_t()) {}
  val_t insert(key_t key, val_t value, insert_type_t type) override
  {
    val_t ret = matching_engine_map_t::insert(key, value, type);
    if (type == insert_type_t::send && !ret) ++nsends_queued;
    return ret;
  }
};

// Send one rendezvous message from send_buffer (on send_device, default if
// empty) to recv_buffer (on the default device) and check it. If !recv_first,
// the receive is posted after the RTS is queued.
void sendrecv(void* send_buffer, lci::mr_t send_mr, void* recv_buffer,
              lci::mr_t recv_mr, size_t size, bool recv_first,
              lci::device_t send_device = lci::device_t())
{
  const int rank = lci::get_rank_me();
  constexpr lci::tag_t tag = 0x2a;
  auto* engine = new counting_engine_t;
  engine->register_rhandler(lci::get_g_runtime());
  lci::matching_engine_t matching_engine(engine);
  lci::comp_t send_cq = lci::alloc_cq();
  lci::comp_t recv_cq = lci::alloc_cq();
  util::write_buffer(send_buffer, size, 's');
  util::write_buffer(recv_buffer, size, 'r');

  auto post_recv = [&]() {
    lci::status_t status = post_until_not_retry(
        [&]() {
          return lci::post_recv_x(rank, recv_buffer, size, tag, recv_cq)
              .matching_engine(matching_engine)
              .mr(recv_mr)();
        },
        send_device);
    EXPECT_TRUE(status.is_posted());
  };
  if (recv_first) post_recv();
  lci::status_t status = post_until_not_retry(
      [&]() {
        return lci::post_send_x(rank, send_buffer, size, tag, send_cq)
            .device(send_device.is_empty() ? default_device() : send_device)
            .matching_engine(matching_engine)
            .mr(send_mr)();
      },
      send_device);
  EXPECT_TRUE(status.is_posted());
  if (!recv_first) {
    wait_until([&]() { return engine->nsends_queued == 1; }, send_device);
    post_recv();
  }
  EXPECT_TRUE(wait_cq(send_cq, send_device).is_done());
  status = wait_cq(recv_cq, send_device);
  EXPECT_TRUE(status.is_done());
  EXPECT_EQ(status.size, size);
  if (status.is_done()) util::check_buffer(recv_buffer, size, 's');

  lci::free_comp(&recv_cq);
  lci::free_comp(&send_cq);
  lci::free_matching_engine(&matching_engine);
}

// Send one rendezvous active message from send_device (default if empty) and
// return the received status.
lci::status_t am(void* send_buffer, lci::mr_t send_mr, size_t size,
                 lci::device_t send_device = lci::device_t())
{
  const int rank = lci::get_rank_me();
  lci::comp_t send_cq = lci::alloc_cq();
  lci::comp_t recv_cq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(recv_cq);
  util::write_buffer(send_buffer, size, 'a');

  lci::status_t status = post_until_not_retry(
      [&]() {
        return lci::post_am_x(rank, send_buffer, size, send_cq, rcomp)
            .device(send_device.is_empty() ? default_device() : send_device)
            .mr(send_mr)();
      },
      send_device);
  EXPECT_TRUE(status.is_posted());
  EXPECT_TRUE(wait_cq(send_cq, send_device).is_done());
  status = wait_cq(recv_cq, send_device);
  EXPECT_TRUE(status.is_done());
  EXPECT_EQ(status.size, size);
  if (status.is_done() && status.size == size)
    util::check_buffer(status.buffer, size, 'a');

  lci::deregister_rcomp(rcomp);
  lci::free_comp(&recv_cq);
  lci::free_comp(&send_cq);
  return status;
}

// Hands out rendezvous buffers from one region registered on one device, with
// its mr when asked for that device.
struct registered_allocator_t : public lci::allocator_base_t {
  registered_buffer_t pool;
  lci::device_t device;
  size_t next = 0;
  int nallocs_with_mr = 0;

  registered_allocator_t(size_t size, lci::device_t device_)
      : pool(size, device_), device(device_)
  {
  }
  void* allocate(size_t size) override { return malloc(size); }
  void* allocate_with_mr(size_t size, lci::device_t device_,
                         lci::mr_t* mr) override
  {
    *mr = lci::MR_HOST;
    if (!(device_ == device) || next + size > pool.size) return allocate(size);
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

// The matching engine is shared by all devices, so the receive's mr (default
// device) and the send's mr (d2) can each end up on a rendezvous step handled
// by the other device. That one side must register on the fly.
TEST(RDV_MR, sendrecv_user_mr_other_device)
{
  lci::g_runtime_init();
  const size_t size = lci::get_max_bcopy_size() + 1024;
  {
    lci::device_t d2 = alloc_second_device();
    {
      registered_buffer_t send_buf(size, d2), recv_buf(size);
      for (bool recv_first : {true, false}) {
        uint64_t nmrs = get_nmrs_registered(d2);
        sendrecv(send_buf.buffer(), send_buf.mr, recv_buf.buffer(), recv_buf.mr,
                 size, recv_first, d2);
        EXPECT_EQ(get_nmrs_registered(d2) - nmrs, 1u);
      }
    }
    lci::free_device(&d2);
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
    lci::device_t d2 = alloc_second_device();
    // The am arrives on the device it was sent from; the allocator is asked
    // for an mr on that device.
    for (lci::device_t device : {default_device(), d2}) {
      registered_buffer_t send_buf(size, device);
      // Room for two messages; the second starts inside the region.
      registered_allocator_t allocator(2 * size + 64, device);
      allocator.next = 64;
      lci::set_allocator(&allocator);
      for (int i = 0; i < 2; ++i) {
        uint64_t nmrs = get_nmrs_registered(d2);
        lci::status_t status = am(send_buf.buffer(), send_buf.mr, size, device);
        EXPECT_EQ(get_nmrs_registered(d2) - nmrs, 0u);
        EXPECT_TRUE(allocator.owns(status.buffer));
        EXPECT_EQ(allocator.nallocs_with_mr, i + 1);
        allocator.deallocate(status.buffer);
      }
      lci::set_allocator(&lci::g_allocator_default);
    }
    lci::free_device(&d2);
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
