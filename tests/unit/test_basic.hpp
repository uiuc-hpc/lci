TEST(AllocFree, runtime)
{
  lcixx::g_runtime_init();
  lcixx::g_runtime_fina();
}

TEST(AllocFree, net_device)
{
  lcixx::g_runtime_init();
  auto device = lcixx::alloc_net_device();
  lcixx::free_net_device(&device);
  ASSERT_TRUE(device.is_empty());
  lcixx::g_runtime_fina();
}

TEST(AllocFree, net_endpoint)
{
  lcixx::g_runtime_init();
  auto endpoint = lcixx::alloc_net_endpoint();
  lcixx::free_net_endpoint(&endpoint);
  ASSERT_TRUE(endpoint.is_empty());
  lcixx::g_runtime_fina();
}