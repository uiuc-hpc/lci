#ifndef FULT_INL_H_
#define FULT_INL_H_

#include "macro.h"
#include "lock.h"

// Fthread.

#define MUL64(x) ((x) << 6)
#define DIV64(x) ((x) >> 6)
#define DIV512(x) ((x) >> 9)
#define DIV32768(x) ((x) >> 15)
#define MUL8(x) ((x) << 3)
#define MOD_POW2(x, y) ((x) & ((y)-1))

MV_INLINE void fthread_create(fthread* f, ffunc func, intptr_t data, size_t stack_size) {
  if (f->stack.sp == NULL) {
    fthread_stack.allocate(f->stack, stack_size);
  }
  f->func = func;
  f->data = data;
  f->ctx.stack_ctx = make_fcontext(f->stack.sp, f->stack.size, fwrapper);
  f->state = CREATED;
}

MV_INLINE void fthread_yield(fthread* f) {
  f->state = YIELD;
  f->ctx.swap_ctx_parent();
}

MV_INLINE void fthread_wait(fthread* f) {
  f->state = BLOCKED;
  f->ctx.swap_ctx_parent();
}

MV_INLINE void fthread_resume(fthread *f) { fworker_sched_thread(f->origin, f->id); }
MV_INLINE void fthread_fini(fthread* f) { fworker_fini_thread(f->origin, f->id); }

MV_INLINE void fthread_join(fthread *f) {
  while (f->state != INVALID) {
    fthread_yield(tlself.thread);
  }
}

MV_INLINE void fwrapper(intptr_t args) {
  fthread* f = (fthread*)args;
  f->func(f->data);
  f->state = INVALID;
  f->ctx.swap_ctx_parent();
}

/// Fworker.

MV_INLINE void fworker_init(fworker** w) {
  // posix_memalign((void**) w, 64, sizeof(fworker));
  *w = new fworker();
  (*w)->stop = true;
  // (*w)->threads = (fthread*) malloc(sizeof(fthread) * NMASK * WORDSIZE);
  posix_memalign((void**)&((*w)->threads), 64, sizeof(fthread) * NMASK * WORDSIZE);
  for (int i = 0; i < NMASK; i++) (*w)->mask[i] = 0;
#ifdef USE_L1_MASK
  for (int i = 0; i < 8; i++) (*w)->l1_mask[i] = 0;
#endif
  // Add all free slot.
  memset((*w)->threads, 0, sizeof(fthread) * (NMASK * WORDSIZE));
  for (int i = (int)(NMASK * WORDSIZE) - 1; i >= 0; i--) {
    fthread_init((*w)->threads);
    (*w)->threads[i].origin = *w;
    (*w)->threads[i].id = i;
    (*w)->thread_pool.push(&((*w)->threads[i]));
  }
  (*w)->thread_pool_lock = MV_SPIN_UNLOCKED;
}

MV_INLINE void fworker_destroy(fworker* w) { free((void*)w->threads); }

MV_INLINE void fworker_fini_thread(fworker* w, const int id) {
  mv_spin_lock(&w->thread_pool_lock);
  w->thread_pool.push(&w->threads[id]);
  mv_spin_unlock(&w->thread_pool_lock);
}

MV_INLINE fthread* fworker_spawn(fworker *w, ffunc f, intptr_t data, size_t stack_size) {
  mv_spin_lock(&w->thread_pool_lock);
  if (w->thread_pool.empty()) {
    throw std::runtime_error("Too many threads are spawn");
  }
  fthread* t = w->thread_pool.top();
  w->thread_pool.pop();
  mv_spin_unlock(&w->thread_pool_lock);

  // add it to the fthread.
  fthread_create(t, f, data, stack_size);

  // make it schedable.
  fworker_sched_thread(w, t->id);

  return t;
}

MV_INLINE void fworker_work(fworker* w, fthread* f) {
  if (xunlikely(f->state == INVALID)) return;
  tlself.thread = f;
  w->ctx.swap_ctx(&f->ctx, (intptr_t)f);
  tlself.thread = NULL;
  if (f->state == YIELD)
    fthread_resume(f);
  else if (f->state == INVALID)
    fthread_fini(f);
}

MV_INLINE void fworker_sched_thread(fworker* w, const int id) {
  sync_set_bit(MOD_POW2(id, WORDSIZE), &w->mask[DIV64(id)]);
#ifdef USE_L1_MASK
  sync_set_bit(DIV512(MOD_POW2(id, 32768)), &w->l1_mask[DIV32768(id)]);
#endif
}

static MV_INLINE int pop_work(unsigned long& mask) {
  auto id = find_first_set(mask);
  bit_flip(mask, id);
  return id;
}

#ifdef ENABLE_STEAL
fworker * random_worker();
#endif

#ifndef USE_L1_MASK
MV_INLINE void wfunc(fworker* w) {
  w->id = nfworker_.fetch_add(1);
  tlself.worker = w;
#ifdef USE_AFFI
  affinity::set_me_to(w->id);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop)) {
#ifdef ENABLE_STEAL
    bool has_work = false;
#endif
    for (auto i = 0; i < NMASK; i++) {
      auto& mask = w->mask[i];
      if (mask > 0) {
        // Atomic exchange to get the current waiting threads.
        auto local_mask = exchange((unsigned long)0, &(mask));
        // Works until it no thread is pending.
        while (xlikely(local_mask > 0)) {
#ifdef ENABLE_STEAL
          has_work = true;
#endif
          int id = pop_work(local_mask);
          // Optains the associate thread.
          fthread* f = &(w->threads[MUL64(i) + id]);
          fworker_work(w, f);
        }
      }
    }

#ifdef ENABLE_STEAL
    if (!has_work && nfworker_ > 1) {
      // Steal..
      fworker* steal = random_worker();
      for (auto i = 0; i < NMASK; i++) {
        auto& mask = steal->mask_[i];
        if (mask > 0) {
          // Atomic exchange to get the current waiting threads.
          auto local_mask = exchange((unsigned long)0, &(mask));
          // Works until it no thread is pending.
          while (xlikely(local_mask > 0)) {
            auto id = pop_work(local_mask);
            // Optains the associate thread.
            fthread* f = &(steal->threads[MUL64(i) + id]);
            fworker_work(w, f);
          }
          break;
        }
      }
    }
#endif
  }
  nfworker_--;

#ifdef USE_PAPI
  wp.stop();
  wp.print();
#endif
}

#else

MV_INLINE void fworker::wfunc(fworker* w) {
  w->id = nfworker_.fetch_add(1);
  tlself.worker = w;
#ifdef USE_AFFI
  affinity::set_me_to(w->id);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop)) {
    for (int l1i = 0; l1i < 8; l1i++) {
      if (w->l1_mask[l1i] == 0) continue;
      auto local_l1_mask = exchange((unsigned long)0, &(w->l1_mask[l1i]));

      while (local_l1_mask > 0) {
        auto ii = find_first_set(local_l1_mask);
        bit_flip(local_l1_mask, ii);

        auto start_i = MUL8(MUL64(l1i)) + MUL8(ii);
        for (auto i = start_i; i < start_i + 8 && i < NMASK; i++) {
          auto& mask = w->mask[i];
          if (mask > 0) {
            unsigned long local_mask = 0;
            // Atomic exchange to get the current waiting threads.
            local_mask = exchange(local_mask, &(w->mask[i]));
            // Works until it no thread is pending.
            while (xlikely(local_mask > 0)) {
              auto id = pop_work(local_mask);
              // Optains the associate thread.
              fthread* f = &w->thread[MUL64(i) + id];
              fworker_work(w, f);
            }
          }
        }
      }
    }
  }
  nfworker_--;

#ifdef USE_PAPI
  wp.stop();
  wp.print();
#endif
}
#endif  // ifndef L1_MASK




#endif
