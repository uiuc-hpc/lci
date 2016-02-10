#ifndef _FULT_H_
#define _FULT_H_

#include <atomic>
#include <boost/lockfree/stack.hpp>
#include <sys/mman.h>

#include "bitops.h"
#include "rdmax.h"
#include "affinity.h"
#include "profiler.h"

#define DEBUG(x)

#define xlikely(x) __builtin_expect((x), 1)
#define xunlikely(x) __builtin_expect((x), 0)

#define fult_yield()           \
  {                            \
    if (t_ctx != NULL)         \
      ((fult*)t_ctx)->yield(); \
    else                       \
      sched_yield();           \
  }

#define fult_wait() \
  { ((fult*)t_ctx)->wait(); }

#define F_STACK_SIZE (4096)
#define MAIN_STACK_SIZE (16*1024)
#define NMASK 2
#define WORDSIZE (8 * sizeof(long))

#define MEMFENCE asm volatile("" : : : "memory")

typedef void (*ffunc)(intptr_t);
static void fwrapper(intptr_t);

class fctx;

__thread fctx* t_ctx = NULL;
__thread int myid;

typedef void* fcontext_t;

extern "C" {
fcontext_t make_fcontext(void* sp, size_t size, void (*thread_func)(intptr_t));
void* jump_fcontext(fcontext_t* old, fcontext_t, intptr_t arg);
}

class fctx {
 public:
  inline void swap(fctx* to) {
    t_ctx = to;
    to->parent_ = this;
    DEBUG(printf("%p --> %p\n", this->myctx_, to->myctx_);)
    jump_fcontext(&(this->myctx_), to->myctx_, (intptr_t)to);
  }

  inline void ret() {
    t_ctx = parent_;
    DEBUG(printf("%p --> %p\n", this, parent_);)
    jump_fcontext(&(this->myctx_), parent_->myctx_, (intptr_t)parent_);
  }

  inline fctx* parent() { return parent_; }

 protected:
  fctx* parent_;
  fcontext_t myctx_;
};

enum fult_state {
  INVALID,
  CREATED,
  // READY, -- this may not be needed.
  YIELD,
  BLOCKED
};

class fult : public fctx {
 public:
  fult() : state_(INVALID), stack(NULL) {}

  ~fult() {
    if (stack != NULL) std::free(stack);
  }

  inline void set(ffunc myfunc, intptr_t data, size_t stack_size) {
    if (stack == NULL) stack = std::malloc(stack_size);
    myfunc_ = myfunc;
    data_ = data;
    state_ = CREATED;
    myctx_ = make_fcontext((void*)((uintptr_t)stack + stack_size),
                           F_STACK_SIZE, fwrapper);
  }

  inline void yield() {
    state_ = YIELD;
    this->ret();
  }

  inline void wait() {
    state_ = BLOCKED;
    this->ret();
  }

  inline void run() {
    (*this->myfunc_)(this->data_);
    // when finish, needs to swap back to the parent.
    this->state_ = INVALID;
    this->ret();
  }

  inline void set_state(fult_state s) { state_ = s; }

  inline fult_state state() { return state_; }

  inline bool is_done() { return state_ == INVALID; }

  using fctx::swap;

 private:
  ffunc myfunc_;
  intptr_t data_;
  volatile fult_state state_;
  void* stack;

} __attribute__((aligned(8)));

static void fwrapper(intptr_t f) {
  fult* ff = (fult*)f;
  ff->run();
}

class worker : fctx {
 public:
  worker() {
    // allocator.allocate(stack, F_STACK_SIZE);
    // stack = malloc(F_STACK_SIZE);
    // myctx_ = make_fcontext(stack, F_STACK_SIZE, fwrapper);
    stop_ = false;

    // Reset all mask.
    for (int i = 0; i < NMASK; i++) mask_[i] = 0;

    // Add all free slot.
    for (int i = (int)(NMASK * WORDSIZE)-1; i>=0; i--) fqueue.push(i);
  }

  inline int spawn(ffunc f, intptr_t data, size_t stack_size = F_STACK_SIZE) {
    int id = -1;
    while (!fqueue.pop(id)) {
      fult_yield();
    }
    fult_new(id, f, data, stack_size);
    return id;
  }

  inline int spawn_to(int tid, ffunc f, intptr_t data, size_t stack_size = F_STACK_SIZE) {
    int id = -1;
    while (fqueue.pop(id)) {
      if (id == tid) break;
      fqueue.push(id);
    }
    fult_new(id, f, data, stack_size);
    return id;
  }

  inline void join(int id) {
    while (!lwt_[id].is_done()) {
        fult_yield();
#if 0
      if (t_ctx != NULL && lwt_[id].state() == CREATED) {
        // we directly schedule it here to avoid going back to scheduler.
        deschedule(id);

        // save the old value so we can switch back to it.
        int saved_id = myid;
        myid = id;

        // really switch ctx.
        t_ctx->swap(&lwt_[id]);

        // now returns.
        myid = saved_id;
      }
#endif
    }
    fqueue.push(id);
  }

  inline void set_state(const int id, fult_state state) {
    lwt_[id].set_state(state);
  }

  inline void schedule(const int id) {
    sync_set_bit(id & (WORDSIZE - 1), &mask_[id >> 6]);
  }

  inline void deschedule(const int id) {
    sync_clear_bit(id & (WORDSIZE - 1), &mask_[id >> 6]);
  }

  inline void start() { w_ = std::thread(wfunc, this); }
  inline void stop() { stop_ = true; w_.join(); }

  inline void start_main(ffunc main_task, intptr_t data) {
    spawn(main_task, data, MAIN_STACK_SIZE);
    wfunc(this);
  }

  inline void stop_main() { stop_ = true; }

  inline bool is_stop() { return stop_; }
  static void wfunc(worker*);

 protected:
  inline void fult_new(const int id, ffunc f, intptr_t data, size_t stack_size) {
    // add it to the fult.
    lwt_[id].set(f, data, stack_size);

    // make it schedable.
    schedule(id);
  }

 private:
  volatile unsigned long mask_[NMASK];
  volatile bool stop_;
  fult lwt_[WORDSIZE * NMASK];
  std::thread w_;
  void* stack;

  // TODO(danghvu): this is temporary, but it is most generic.
  boost::lockfree::stack<uint8_t, boost::lockfree::capacity<WORDSIZE * NMASK>>
      fqueue;
};

#define doschedule(i)                                              \
  {                                                                \
    myid = i * WORDSIZE + id;                                      \
    if (w->lwt_[myid].state() != INVALID) w->swap(&w->lwt_[myid]); \
    if (w->lwt_[myid].state() == YIELD) w->schedule(myid);         \
  }

#define loop_sched_all(mask, i)                                         \
  {                                                                     \
    register unsigned long local = exchange((unsigned long)0, &(mask)); \
    while (local > 0) {                                                 \
      int id = find_first_set(local);                                   \
      local ^= ((unsigned long)1 << id);                                \
      doschedule(i);                                                    \
    }                                                                   \
  }

std::atomic<int> nworker;

void worker::wfunc(worker* w) {
#ifdef USE_AFFI
  affinity::set_me_to(nworker++);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->is_stop())) {
    for (auto i=0; i<NMASK; i++) {
      auto& mask = w->mask_[i];
      if (mask > 0) {
        loop_sched_all(mask, i);
      }
    }
  }

#ifdef USE_PAPI
  wp.stop();
  wp.print();
#endif
}

class fult_sync {
 public:
  inline fult_sync() : ctx_(NULL), parent_(NULL), id_(myid) {
    if (t_ctx != NULL) {
      ctx_ = static_cast<fult*>(t_ctx);
      parent_ = reinterpret_cast<worker*>(ctx_->parent());
    }
  }

  inline bool has_ctx() { return ctx_ != NULL; }

  inline void wait() {
    ctx_->wait();
  }

  inline void signal() {
    parent_->schedule(id_);
  }

 private:
  fult* ctx_;
  worker* parent_;
  int id_;

} __attribute__((aligned));

#endif
