TEST(AllocFree, runtime)
{
  lcixx::g_runtime_init_x().call();
  lcixx::g_runtime_fina_x().call();
}

TEST(AllocFree, net_device)
{
  lcixx::g_runtime_init_x().call();
  lcixx::net_device_t device;
  lcixx::alloc_net_device_x(&device).call();
  lcixx::free_net_device_x(&device).call();
  ASSERT_EQ(device.p_impl, nullptr);
  lcixx::g_runtime_fina_x().call();
}

TEST(AllocFree, net_endpoint)
{
  lcixx::g_runtime_init_x().call();
  lcixx::net_endpoint_t endpoint;
  lcixx::alloc_net_endpoint_x(&endpoint).call();
  lcixx::free_net_endpoint_x(&endpoint).call();
  ASSERT_EQ(endpoint.p_impl, nullptr);
  lcixx::g_runtime_fina_x().call();
}