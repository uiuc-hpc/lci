#include <gtest/gtest.h>
#include "lcixx.hpp"
#include "test_basic.hpp"
#include "test_network.hpp"
#include "test_mpmc_array.hpp"
#include "test_mpmc_set.hpp"
#include "test_cq.hpp"

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}