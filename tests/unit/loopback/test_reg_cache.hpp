// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <cstdlib>
#include <unistd.h>

#if LCI_USE_REG_CACHE

TEST(RegCache, rmr_preserves_user_base_for_subrange)
{
  lci::g_runtime_init();

  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  ASSERT_GT(page_size, 0u);

  void* allocation = nullptr;
  ASSERT_EQ(posix_memalign(&allocation, page_size, page_size * 4), 0);
  ASSERT_NE(allocation, nullptr);

  char* user_base = static_cast<char*>(allocation) + 848;
  constexpr size_t user_size = 8117;

  lci::mr_t mr = lci::register_memory(user_base, user_size);
  lci::rmr_t rmr = lci::get_rmr(mr);

  EXPECT_EQ(rmr.base, reinterpret_cast<uintptr_t>(user_base));
  EXPECT_LE(rmr.mr_base, rmr.base);

  lci::deregister_memory(&mr);
  free(allocation);
  lci::g_runtime_fina();
}

#endif  // LCI_USE_REG_CACHE
