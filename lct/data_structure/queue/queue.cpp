#include <stdexcept>
#include "lcti.hpp"
#include "data_structure/queue/queue_base.hpp"
#include "data_structure/queue/queue_array_atomic_faa.hpp"
#include "data_structure/queue/queue_array_atomic_cas.hpp"
#include "data_structure/queue/queue_array_atomic_basic.hpp"
#include "data_structure/queue/queue_array.hpp"
#include "data_structure/queue/queue_std.hpp"
#include "data_structure/queue/queue_concurrency_freaks.hpp"
// #include "data_structure/queue/queue_faaarray.hpp"
// #include "data_structure/queue/queue_lazy_index.hpp"

LCT_queue_t LCT_queue_alloc(LCT_queue_type_t type, size_t length)
{
  lct::queue_base_t* q;
  switch (type) {
    case LCT_QUEUE_ARRAY_ATOMIC_FAA:
      q = new lct::queue_array_atomic_faa_t(length);
      break;
    case LCT_QUEUE_ARRAY_ATOMIC_CAS:
      q = new lct::queue_array_atomic_cas_t(length);
      break;
    case LCT_QUEUE_ARRAY_ATOMIC_BASIC:
      q = new lct::queue_array_atomic_basic_t(length);
      break;
    case LCT_QUEUE_ARRAY_ST:
      q = new lct::queue_array_t<false>(length);
      break;
    case LCT_QUEUE_ARRAY_MUTEX:
      q = new lct::queue_array_t<true>(length);
      break;
    case LCT_QUEUE_STD:
      q = new lct::queue_std_t<false>();
      break;
    case LCT_QUEUE_STD_MUTEX:
      q = new lct::queue_std_t<true>();
      break;
    case LCT_QUEUE_MS:
      q = new lct::queue_concurrency_freaks_t<MichaelScottQueue<void>>();
      break;
    case LCT_QUEUE_LCRQ:
      q = new lct::queue_concurrency_freaks_t<LCRQueue<void>>();
      break;
    case LCT_QUEUE_LPRQ:
      q = new lct::queue_concurrency_freaks_t<LPRQueue<void>>();
      break;
    case LCT_QUEUE_FAAARRAY:
      q = new lct::queue_concurrency_freaks_t<FAAArrayQueue<void>>();
      break;
    case LCT_QUEUE_LAZY_INDEX:
      q = new lct::queue_concurrency_freaks_t<LazyIndexArrayQueue<void>>();
      break;
    default:
      throw std::runtime_error("unknown queue type " + std::to_string(type));
  }
  return reinterpret_cast<LCT_queue_t>(q);
}
void LCT_queue_free(LCT_queue_t* queue_p)
{
  auto q = reinterpret_cast<lct::queue_base_t*>(*queue_p);
  delete q;
  *queue_p = nullptr;
}
void LCT_queue_push(LCT_queue_t queue, void* val)
{
  auto q = reinterpret_cast<lct::queue_base_t*>(queue);
  q->push(val);
}
void* LCT_queue_pop(LCT_queue_t queue)
{
  auto q = reinterpret_cast<lct::queue_base_t*>(queue);
  return q->pop();
}