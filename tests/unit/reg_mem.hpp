TEST(RegMem, reg_mem)
{
  lcixx::runtime_t runtime = lcixx::alloc_runtime_x().call();
  lcixx::net_device_t device = lcixx::alloc_net_device_x(runtime).call();
  const int size = 1024;
  void* address = malloc(size);
  lcixx::mr_t mr = lcixx::register_memory_x(device, address, size).call();
  lcixx::deregister_memory_x(mr).call();
  free(address);
  lcixx::free_net_device_x(device).call();
  lcixx::free_runtime_x(runtime).call();
}