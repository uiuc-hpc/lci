TEST(AllocFree, runtime)
{
  lcixx::runtime_t runtime = lcixx::alloc_runtime_x().call();
  lcixx::free_runtime_x(runtime).call();
}