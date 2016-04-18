#ifndef SEGMENTED_STACK_H_
#define SEGMENTED_STACK_H_

#include <boost/assert.hpp>
#include <boost/coroutine/stack_context.hpp>

extern "C" {

void* __splitstack_makecontext(std::size_t, void * [BOOST_COROUTINES_SEGMENTS],
                               std::size_t*);

void __splitstack_releasecontext(void * [BOOST_COROUTINES_SEGMENTS]);

void __splitstack_resetcontext(void * [BOOST_COROUTINES_SEGMENTS]);

void __splitstack_block_signals_context(void * [BOOST_COROUTINES_SEGMENTS],
                                        int* new_value, int* old_value);
}

#ifdef BOOST_HAS_ABI_HEADERS
#include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace coroutines {
namespace detail {

inline bool segmented_stack_allocator::is_stack_unbound() { return true; }

inline std::size_t segmented_stack_allocator::minimum_stacksize() {
  return 4096;
}

inline std::size_t segmented_stack_allocator::default_stacksize() {
  return 4096;
}

inline std::size_t segmented_stack_allocator::maximum_stacksize() {
  BOOST_ASSERT_MSG(false, "segmented stack is unbound");
  return 0;
}

inline void segmented_stack_allocator::allocate(stack_context& ctx,
                                                std::size_t size) {
  void* limit = __splitstack_makecontext(size, ctx.segments_ctx, &ctx.size);
  BOOST_ASSERT(limit);
  ctx.sp = static_cast<char*>(limit) + ctx.size;

  int off = 0;
  __splitstack_block_signals_context(ctx.segments_ctx, &off, 0);
}

inline void segmented_stack_allocator::deallocate(stack_context& ctx) {
  __splitstack_releasecontext(ctx.segments_ctx);
}
}
}
}
#endif
