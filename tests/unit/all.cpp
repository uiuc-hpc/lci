#include <gtest/gtest.h>
#include "lcixx.hpp"
#include "test_basic.hpp"
#include "test_network.hpp"
#include "test_mpmc_array.hpp"
#include "test_cq.hpp"
#include "test_packet_pool.hpp"

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}