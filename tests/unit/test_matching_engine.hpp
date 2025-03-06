#include "lci_internal.hpp"
#include <thread>

namespace test_matching_engine
{
using matching_engine_t = lci::matching_engine_map_t;
// using matching_engine_t = lci::matching_engine_queue_t;

TEST(MATCHING_ENGINE, singlethread)
{
  lci::g_runtime_init();
  lci::matching_engine_attr_t attr;
  matching_engine_t mengine(attr);
  const int n = 1000;
  for (int i = 0; i < n; i++) {
    mengine.insert(i, reinterpret_cast<void*>(i+1), lci::matching_engine_impl_t::insert_type_t::send);
  }
  for (int i = n - 1; i >= 0; i--) {
    void* val = mengine.insert(i, reinterpret_cast<void*>(1), lci::matching_engine_impl_t::insert_type_t::recv);
    ASSERT_EQ(reinterpret_cast<uint64_t>(val), i + 1);
  }
  lci::g_runtime_fina();
}

// all threads put and get
void test_multithread0(matching_engine_t& mengine, const std::vector<int>& in, int start, int n, bool out[])
{
  for (uint64_t i = start; i < start + n; i++) {
    uint64_t key = in[i];
    auto type = lci::matching_engine_impl_t::insert_type_t::send;
    if (key >= in.size() / 2)
      type = lci::matching_engine_impl_t::insert_type_t::recv;
    key = key % (in.size() / 2);

    void *val = mengine.insert(key, reinterpret_cast<void*>(key+1), type);
    if (val) {
      ASSERT_EQ(reinterpret_cast<uint64_t>(val), key + 1);
      ASSERT_EQ(out[key], false);
      out[key] = true;
    }
  }
}

TEST(MATCHING_ENGINE, multithread0)
{
  lci::global_initialize();
  const int nthreads = 16;
  const int n = 100000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = 2 * n / nthreads;

  lci::matching_engine_attr_t attr;
  matching_engine_t mengine(attr);
  
  std::vector<int> in(2 * n);
  for (int i = 0; i < 2 * n; i++) {
    in[i] = i;
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(in.begin(), in.end(), g);

  bool out[n];
  memset(out, false, sizeof(out));

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_multithread0, std::ref(mengine), std::ref(in), i * n_per_thread,
                  n_per_thread, out);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < n; i++) {
    ASSERT_EQ(out[i], true);
  }

  lci::global_finalize();
}

}  // namespace test_matching_engine