// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace test_matching_policy
{
TEST(MATCHING_POLICY, test_rank_tag)
{
  lci::g_runtime_init();
  const int n = 1000;

  std::vector<int> in(n);
  for (int i = 0; i < n; i++) {
    in[i] = i;
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(in.begin(), in.end(), g);

  uint64_t data = 0xdeadbeef;
  for (int i = 0; i < n; ++i) {
    lci::status_t status =
        lci::post_send(0, &data, sizeof(data), in[i], lci::COMP_NULL_EXPECT_OK);
    ASSERT_EQ(status.error.is_ok(), true);
  }
  for (int i = 0; i < n; ++i) {
    lci::status_t status =
        lci::post_recv(0, &data, sizeof(data), in[i], lci::COMP_NULL_EXPECT_OK);
    ASSERT_EQ(status.error.is_ok(), true);
    ASSERT_EQ(status.tag, in[i]);
  }

  lci::g_runtime_fina();
}

TEST(MATCHING_POLICY, test_rank_only)
{
  lci::g_runtime_init();
  const int n = 1000;

  std::vector<int> in(n);
  for (int i = 0; i < n; i++) {
    in[i] = i;
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(in.begin(), in.end(), g);

  uint64_t data = 0xdeadbeef;
  for (int i = 0; i < n; ++i) {
    lci::status_t status =
        lci::post_send_x(0, &data, sizeof(data), in[i],
                         lci::COMP_NULL_EXPECT_OK)
            .matching_policy(lci::matching_policy_t::rank_only)();
    ASSERT_EQ(status.error.is_ok(), true);
  }
  bool flags[n];
  memset(flags, false, sizeof(flags));
  for (int i = 0; i < n; ++i) {
    lci::status_t status = lci::post_recv(0, &data, sizeof(data), lci::ANY_TAG,
                                          lci::COMP_NULL_EXPECT_OK);
    ASSERT_EQ(status.error.is_ok(), true);
    int idx = status.tag;
    ASSERT_EQ(idx >= 0 && idx < n, true);
    ASSERT_EQ(flags[idx], false);
    flags[idx] = true;
  }
  for (int i = 0; i < n; ++i) {
    ASSERT_EQ(flags[i], true);
  }

  lci::g_runtime_fina();
}

TEST(MATCHING_POLICY, test_none)
{
  lci::g_runtime_init();
  const int n = 1000;

  std::vector<int> in(n);
  for (int i = 0; i < n; i++) {
    in[i] = i;
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(in.begin(), in.end(), g);

  uint64_t data = 0xdeadbeef;
  for (int i = 0; i < n; ++i) {
    lci::status_t status = lci::post_send_x(0, &data, sizeof(data), in[i],
                                            lci::COMP_NULL_EXPECT_OK)
                               .matching_policy(lci::matching_policy_t::none)();
    ASSERT_EQ(status.error.is_ok(), true);
  }
  bool flags[n];
  memset(flags, false, sizeof(flags));
  for (int i = 0; i < n; ++i) {
    lci::status_t status =
        lci::post_recv(lci::ANY_SOURCE, &data, sizeof(data), lci::ANY_TAG,
                       lci::COMP_NULL_EXPECT_OK);
    ASSERT_EQ(status.error.is_ok(), true);
    ASSERT_EQ(status.rank, 0);
    int idx = status.tag;
    ASSERT_EQ(idx >= 0 && idx < n, true);
    ASSERT_EQ(flags[idx], false);
    flags[idx] = true;
  }
  for (int i = 0; i < n; ++i) {
    ASSERT_EQ(flags[i], true);
  }

  lci::g_runtime_fina();
}

}  // namespace test_matching_policy