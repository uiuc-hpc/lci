#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "lci.hpp"
#include "test_basic.hpp"
#include "test_network.hpp"
#include "test_mpmc_array.hpp"
#include "test_mpmc_set.hpp"
#include "test_cq.hpp"
#include "test_comm.hpp"

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}