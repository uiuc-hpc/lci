TEST(AllocFree, runtime)
{
  lcixx::runtime_t runtime = lcixx::alloc_runtime_x().call();
  lcixx::free_runtime_x(runtime).call();
}

TEST(AllocFree, net_device)
{
  lcixx::runtime_t runtime = lcixx::alloc_runtime_x().call();
  lcixx::net_device_t device = lcixx::alloc_net_device_x(runtime).call();
  lcixx::free_runtime_x(runtime).call();
}