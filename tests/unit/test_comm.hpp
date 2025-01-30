namespace test_comm
{
TEST(COMM, am_st)
{
  lcixx::g_runtime_init_x().call();

  const int nmsgs = 1000;
  int rank, nranks;
  lcixx::get_rank_x(&rank).call();
  lcixx::get_nranks_x(&nranks).call();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lcixx::comp_t lcq;
  lcixx::alloc_cq_x(&lcq).call();

  lcixx::comp_t rcq;
  lcixx::alloc_cq_x(&rcq).call();
  lcixx::rcomp_t rcomp;
  lcixx::register_rcomp_x(rcq, &rcomp).call();
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lcixx::error_t error(lcixx::errorcode_t::retry);
    while (error.is_retry()) {
      lcixx::communicate_x(lcixx::direction_t::SEND, rank, &data, sizeof(data),
                           lcq, &error)
          .remote_comp(rcomp)
          .call();
      lcixx::progress_x().call();
    }
    // poll local cq
    error.reset(lcixx::errorcode_t::retry);
    while (error.is_retry()) {
      lcixx::status_t status;
      lcixx::cq_pop_x(lcq, &error, &status).call();
      lcixx::progress_x().call();
      ASSERT_EQ(*(uint64_t*)status.buffer, data);
    }
    // poll remote cq
    error.reset(lcixx::errorcode_t::retry);
    while (error.is_retry()) {
      lcixx::status_t status;
      lcixx::cq_pop_x(rcq, &error, &status).call();
      lcixx::progress_x().call();
      ASSERT_EQ(*(uint64_t*)status.buffer, data);
    }
  }

  lcixx::free_cq_x(&lcq).call();
  lcixx::free_cq_x(&rcq).call();
  lcixx::g_runtime_fina_x().call();
}

void test_am_mt(int id, int nmsgs, lcixx::comp_t lcq, lcixx::comp_t rcq,
                lcixx::rcomp_t rcomp, uint64_t* p_data)
{
  int rank;
  lcixx::get_rank_x(&rank).call();
  lcixx::tag_t tag = id;

  for (int i = 0; i < nmsgs; i++) {
    lcixx::error_t error(lcixx::errorcode_t::retry);
    while (error.is_retry()) {
      lcixx::communicate_x(lcixx::direction_t::SEND, rank, p_data,
                           sizeof(uint64_t), lcq, &error)
          .remote_comp(rcomp)
          .tag(tag)
          .call();
      lcixx::progress_x().call();
    }
    // poll cqs
    bool lcq_done = false;
    bool rcq_done = false;
    lcixx::status_t status;
    while (!lcq_done || !rcq_done) {
      lcixx::progress_x().call();
      if (!lcq_done) {
        error.reset(lcixx::errorcode_t::retry);
        lcixx::cq_pop_x(lcq, &error, &status).call();
        if (error.is_ok()) {
          ASSERT_EQ(status.buffer, p_data);
          ASSERT_EQ(*(uint64_t*)status.buffer, *p_data);
          // ASSERT_EQ(status.tag, tag);
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        error.reset(lcixx::errorcode_t::retry);
        lcixx::cq_pop_x(rcq, &error, &status).call();
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
  lcixx::g_runtime_init_x().call();

  const int nmsgs = 20000;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank, nranks;
  lcixx::get_rank_x(&rank).call();
  lcixx::get_nranks_x(&nranks).call();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lcixx::comp_t lcq;
  lcixx::alloc_cq_x(&lcq).call();
  // remote cq
  lcixx::comp_t rcq;
  lcixx::alloc_cq_x(&rcq).call();
  lcixx::rcomp_t rcomp;
  lcixx::register_rcomp_x(rcq, &rcomp).call();
  // std::vector<lcixx::comp_t> lcqs;
  // std::vector<lcixx::comp_t> rcqs;
  // std::vector<lcixx::rcomp_t> rcomps;
  // for (int i = 0; i < nthreads; i++) {
  //   lcixx::comp_t lcq;
  //   lcixx::alloc_cq_x(&lcq).call();
  //   lcqs.push_back(lcq);
  //   lcixx::comp_t rcq;
  //   lcixx::alloc_cq_x(&rcq).call();
  //   rcqs.push_back(rcq);
  //   lcixx::rcomp_t rcomp;
  //   lcixx::register_rcomp_x(rcq, &rcomp).call();
  //   rcomps.push_back(rcomp);
  // }

  // uint64_t data = 0xdeadbeef;
  uint64_t data = 1;

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_am_mt, i, nmsgs / nthreads, lcq, rcq, rcomp, &data);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lcixx::free_cq_x(&lcq).call();
  lcixx::free_cq_x(&rcq).call();
  lcixx::g_runtime_fina_x().call();
}

}  // namespace test_comm