#include "lci_internal.hpp"

namespace lci
{
void matching_engine_impl_t::register_rhandler(runtime_t runtime)
{
  rcomp = runtime.p_impl->rhandler_registry.register_rhandler(
      {rhandler_registry_t::type_t::matching_engine, this});
}

matching_engine_t alloc_matching_engine_x::call_impl(runtime_t runtime,
                                                     void* user_context) const
{
  matching_engine_t matching_engine;
  matching_engine_attr_t attr;
  attr.user_context = user_context;
  matching_engine.p_impl = new matching_engine_queue_t(attr);
  matching_engine.get_impl()->register_rhandler(runtime);
  return matching_engine;
}

void free_matching_engine_x::call_impl(matching_engine_t* matching_engine,
                                       runtime_t runtime) const
{
  delete matching_engine->p_impl;
  matching_engine->p_impl = nullptr;
}

}  // namespace lci
