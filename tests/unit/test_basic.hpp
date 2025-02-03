TEST(AllocFree, runtime)
{
  lci::g_runtime_init();
  lci::g_runtime_fina();
}

TEST(AllocFree, net_device)
{
  lci::g_runtime_init();
  auto device = lci::alloc_net_device();
  lci::free_net_device(&device);
  ASSERT_TRUE(device.is_empty());
  lci::g_runtime_fina();
}

TEST(AllocFree, net_endpoint)
{
  lci::g_runtime_init();
  auto endpoint = lci::alloc_net_endpoint();
  lci::free_net_endpoint(&endpoint);
  ASSERT_TRUE(endpoint.is_empty());
  lci::g_runtime_fina();
}