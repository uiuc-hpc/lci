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
  lcixx::g_runtime_fina_x().call();
}