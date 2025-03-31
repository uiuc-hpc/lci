// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <iterator>
#include "lci.hpp"
#include "util.hpp"
#include "test_basic.hpp"
#include "test_network.hpp"
#include "test_mpmc_array.hpp"
#include "test_mpmc_set.hpp"
#include "test_matching_engine.hpp"
#include "test_sync.hpp"
#include "test_cq.hpp"
#include "test_graph.hpp"
#include "test_am.hpp"
#include "test_sendrecv.hpp"
#include "test_put.hpp"
#include "test_putImm.hpp"
#include "test_get.hpp"
#include "test_backlog_queue.hpp"
#include "test_matching_policy.hpp"

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}