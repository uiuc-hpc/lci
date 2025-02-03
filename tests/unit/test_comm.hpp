#include "lcixx_internal.hpp"

namespace test_comm
{
TEST(COMM, am_st)
{
  lcixx::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lcixx::get_rank();
  int nranks = lcixx::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lcixx::comp_t lcq = lcixx::alloc_cq();

  lcixx::comp_t rcq = lcixx::alloc_cq();
  lcixx::rcomp_t rcomp = lcixx::register_rcomp(rcq);
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lcixx::error_t error(lcixx::errorcode_t::retry);
    while (error.is_retry()) {
      lcixx::status_t status;
      std::tie(error, status) =
          lcixx::communicate_x(lcixx::direction_t::SEND, rank, &data,
                               sizeof(data), lcq)
              .remote_comp(rcomp)();
      lcixx::progress_x().call();
    }
    // poll local cq
    error.reset(lcixx::errorcode_t::retry);
    lcixx::status_t status;
    while (error.is_retry()) {
      lcixx::progress_x().call();
      std::tie(error, status) = lcixx::cq_pop(lcq);
    }
    ASSERT_EQ(*(uint64_t*)status.buffer, data);
    // poll remote cq
    error.reset(lcixx::errorcode_t::retry);
    while (error.is_retry()) {
      lcixx::status_t status;
      std::tie(error, status) = lcixx::cq_pop(rcq);
      lcixx::progress_x().call();
    }
    ASSERT_EQ(*(uint64_t*)status.buffer, data);
  }

  lcixx::free_cq(&lcq);
  lcixx::free_cq(&rcq);
  lcixx::g_runtime_fina();
}

void test_am_mt(int id, int nmsgs, lcixx::comp_t lcq, lcixx::comp_t rcq,
                lcixx::rcomp_t rcomp, uint64_t* p_data)
{
  int rank = lcixx::get_rank();
  lcixx::tag_t tag = id;

  for (int i = 0; i < nmsgs; i++) {
    lcixx::error_t error(lcixx::errorcode_t::retry);
    while (error.is_retry()) {
      lcixx::status_t status;
      std::tie(error, status) =
          lcixx::communicate_x(lcixx::direction_t::SEND, rank, p_data,
                               sizeof(uint64_t), lcq)
              .remote_comp(rcomp)
              .tag(tag)();
      lcixx::progress();
    }
    // poll cqs
    bool lcq_done = false;
    bool rcq_done = false;
    lcixx::status_t status;
    while (!lcq_done || !rcq_done) {
      lcixx::progress();
      if (!lcq_done) {
        error.reset(lcixx::errorcode_t::retry);
        std::tie(error, status) = lcixx::cq_pop(lcq);
        if (error.is_ok()) {
          ASSERT_EQ(status.buffer, p_data);
          ASSERT_EQ(*(uint64_t*)status.buffer, *p_data);
          // ASSERT_EQ(status.tag, tag);
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        error.reset(lcixx::errorcode_t::retry);
        std::tie(error, status) = lcixx::cq_pop(rcq);
        if (error.is_ok()) {
          ASSERT_EQ(*(uint64_t*)status.buffer, *p_data);
          // ASSERT_EQ(status.tag, tag);
          free(status.buffer);
          rcq_done = true;
        }
      }
    }
  }
}

TEST(COMM, am_mt)
{
  lcixx::g_runtime_init();

  const int nmsgs = 20000;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lcixx::get_rank();
  int nranks = lcixx::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  lcixx::comp_t lcq = lcixx::alloc_cq();
  lcixx::comp_t rcq = lcixx::alloc_cq();
  lcixx::rcomp_t rcomp = lcixx::register_rcomp(rcq);
  // std::vector<lcixx::comp_t> lcqs;
  // std::vector<lcixx::comp_t> rcqs;
  // std::vector<lcixx::rcomp_t> rcomps;
  // for (int i = 0; i < nthreads; i++) {
  // lcqs.push_back(lcixx::alloc_cq());
  // rcqs.push_back(lcixx::alloc_cq());
  // rcomps.push_back(lcixx::register_rcomp(rcqs[i]));
  // }

  // uint64_t data = 0xdeadbeef;
  uint64_t data = 1;

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_am_mt, i, nmsgs / nthreads, lcq, rcq, rcomp, &data);
    // std::thread t(test_am_mt, i, nmsgs / nthreads, lcqs[i], rcqs[i],
    // rcomps[i], &data);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lcixx::free_cq(&lcq);
  lcixx::free_cq(&rcq);
  // for (int i = 0; i < nthreads; i++) {
  // lcixx::deregister_rcomp(rcomps[i]);
  // lcixx::free_cq(&rcqs[i]);
  // lcixx::free_cq(&lcqs[i]);
  // }
  lcixx::g_runtime_fina();
}

}  // namespace test_comm