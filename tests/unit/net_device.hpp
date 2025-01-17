TEST(NET_DEVICE, reg_mem)
{
  lcixx::g_runtime_init_x().call();
  lcixx::net_device_t device;
  lcixx::alloc_net_device_x(&device).call();
  const int size = 1024;
  void* address = malloc(size);
  lcixx::mr_t mr;
  lcixx::register_memory_x(device, address, size, &mr).call();
  lcixx::deregister_memory_x(mr).call();
  free(address);
  lcixx::free_net_device_x(device).call();
  lcixx::g_runtime_fina_x().call();
}

TEST(NET_DEVICE, poll_cq)
{
  lcixx::g_runtime_init_x().call();
  lcixx::net_device_t device;
  lcixx::alloc_net_device_x(&device).call();
  std::vector<lcixx::net_status_t> statuses;
  lcixx::net_poll_cq_x(device, &statuses).call();
  ASSERT_EQ(statuses.size(), 0);
  lcixx::free_net_device_x(device).call();
  lcixx::g_runtime_fina_x().call();
}