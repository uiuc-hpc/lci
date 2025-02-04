#include "lci_internal.hpp"

namespace test_comm
{
TEST(COMM, am_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t lcq = lci::alloc_cq();

  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lci::status_t status;
    do {
      status =
          lci::post_am(rank, &data, sizeof(data), lcq, rcomp);
      lci::progress_x().call();
    } while (status.error.is_retry());
    // poll local cq
    do {
      lci::progress_x().call();
      status = lci::cq_pop(lcq);
    } while (status.error.is_retry());
    ASSERT_EQ(*(uint64_t*)status.buffer, data);
    // poll remote cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress_x().call();
    } while (status.error.is_retry());
    ASSERT_EQ(*(uint64_t*)status.buffer, data);
  }

  lci::free_cq(&lcq);
  lci::free_cq(&rcq);
  lci::g_runtime_fina();
}

void test_am_mt(int id, int nmsgs, lci::comp_t lcq, lci::comp_t rcq,
                lci::rcomp_t rcomp, uint64_t* p_data)
{
  int rank = lci::get_rank();
  lci::tag_t tag = id;

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status =
          lci::post_am_x(rank, p_data, sizeof(uint64_t), lcq, rcomp).tag(tag)();
      lci::progress();
    } while (status.error.is_retry());
    // poll cqs
    bool lcq_done = false;
    bool rcq_done = false;
    while (!lcq_done || !rcq_done) {
      lci::progress();
      if (!lcq_done) {
        status = lci::cq_pop(lcq);
        if (status.error.is_ok()) {
          ASSERT_EQ(status.buffer, p_data);
          ASSERT_EQ(*(uint64_t*)status.buffer, *p_data);
          // ASSERT_EQ(status.tag, tag);
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        status = lci::cq_pop(rcq);
        if (status.error.is_ok()) {
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
  lci::g_runtime_init();

  const int nmsgs = 20000;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  // std::vector<lci::comp_t> lcqs;
  // std::vector<lci::comp_t> rcqs;
  // std::vector<lci::rcomp_t> rcomps;
  // for (int i = 0; i < nthreads; i++) {
  // lcqs.push_back(lci::alloc_cq());
  // rcqs.push_back(lci::alloc_cq());
  // rcomps.push_back(lci::register_rcomp(rcqs[i]));
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

  lci::free_cq(&lcq);
  lci::free_cq(&rcq);
  // for (int i = 0; i < nthreads; i++) {
  // lci::deregister_rcomp(rcomps[i]);
  // lci::free_cq(&rcqs[i]);
  // lci::free_cq(&lcqs[i]);
  // }
  lci::g_runtime_fina();
}

}  // namespace test_comm