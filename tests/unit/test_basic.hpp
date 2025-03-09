// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

TEST(AllocFree, runtime)
{
  lci::g_runtime_init();
  lci::g_runtime_fina();
}

TEST(AllocFree, device)
{
  lci::g_runtime_init();
  auto device = lci::alloc_device();
  lci::free_device(&device);
  ASSERT_TRUE(device.is_empty());
  lci::g_runtime_fina();
}

TEST(AllocFree, endpoint)
{
  lci::g_runtime_init();
  auto endpoint = lci::alloc_endpoint();
  lci::free_endpoint(&endpoint);
  ASSERT_TRUE(endpoint.is_empty());
  lci::g_runtime_fina();
}