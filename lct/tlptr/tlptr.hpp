#include <vector>
#include <atomic>
#include "lcti.hpp"

namespace lct::tlptr
{
using ptr_t = void*;
struct tlptr_t {
  tlptr_t();
  ~tlptr_t();
  ptr_t get();
  void set(ptr_t);
  std::vector<ptr_t> get_all();
  template <typename T>
  T* get_or_allocate()
  {
    ptr_t ptr = get();
    if (ptr == nullptr) {
      ptr = new T;
      set(ptr);
    }
    return reinterpret_cast<T*>(ptr);
  }

 private:
  uint64_t id;
  spinlock_t lock;
  std::vector<ptr_t> allptrs;
};
}  // namespace lct::tlptr
