#ifndef FULT_INL_H_
#define FULT_INL_H_

#include <string.h>

#include "mv/lock.h"
#include "mv/macro.h"
#include "mv/affinity.h"

// Fthread.

#define MUL64(x) ((x) << 6)
#define DIV64(x) ((x) >> 6)
#define DIV512(x) ((x) >> 9)
#define DIV32768(x) ((x) >> 15)
#define MUL8(x) ((x) << 3)
#define MOD_POW2(x, y) ((x) & ((y)-1))

#define MAX_THREAD (NMASK * WORDSIZE)

MV_INLINE void fthread_create(fthread* f, ffunc func, intptr_t data,
                              size_t stack_size)
{
  if (unlikely(f->stack == NULL)) {
    void* memory = 0;
    posix_memalign(&memory, 64, stack_size);
    if (memory == 0) {
      printf("No more memory for stack\n");
      exit(EXIT_FAILURE);
    }
    f->stack = (void*) ((uintptr_t) memory + stack_size);
  }
  f->func = func;
  f->data = data;
  f->ctx.stack_ctx = make_fcontext(f->stack, stack_size, fwrapper);
  f->state = CREATED;
}

MV_INLINE void fthread_yield(fthread* f)
{
  f->state = YIELD;
  swap_ctx_parent(&f->ctx);
}

MV_INLINE void fthread_wait(fthread* f)
{
  f->state = BLOCKED;
  swap_ctx_parent(&f->ctx);
}

MV_INLINE void fthread_resume(fthread* f)
{
  fworker_sched_thread(f->origin, f->id);
}

MV_INLINE void fthread_fini(fthread* f)
{
  fworker_fini_thread(f->origin, f->id);
}

MV_INLINE void fthread_join(fthread* f)
{
  while (f->state != INVALID) {
    fthread_yield(tlself.thread);
  }
}

static void fwrapper(intptr_t args)
{
  fthread* f = (fthread*)args;
  f->func(f->data);
  f->state = INVALID;
  swap_ctx_parent(&f->ctx);
}

/// Fworker.

MV_INLINE void fworker_init(fworker** w_ptr)
{
#ifndef __cplusplus
  fworker* w = 0;
  posix_memalign((void**) &w, 64, sizeof(fworker));
#else
  fworker* w = new fworker(); // Work around... malloc fail here for no reason.
#endif

  w->stop = 1;
  posix_memalign((void**)&(w->threads), 64,
                 sizeof(fthread) * MAX_THREAD);
  w->thread_pool = (fthread**) malloc( MAX_THREAD * sizeof(uintptr_t));
  memset(w->threads, 0, sizeof(fthread) * MAX_THREAD);
  memset(w->thread_pool, 0, sizeof(fthread*) * MAX_THREAD);

  for (int i = 0; i < NMASK; i++) w->mask[i] = 0;
#ifdef USE_L1_MASK
  for (int i = 0; i < 8; i++) w->l1_mask[i] = 0;
#endif
  // Add all free slot.
  for (int i = (int)(MAX_THREAD) - 1; i >= 0; i--) {
    fthread_init(&(w->threads[i]));
    w->threads[i].origin = w;
    w->threads[i].id = i;
    w->thread_pool[w->thread_pool_last++] = (&(w->threads[i]));
  }
  w->thread_pool_lock = MV_SPIN_UNLOCKED;

  *w_ptr = w;
}

MV_INLINE void fworker_destroy(fworker* w) { free((void*)w->threads); }
MV_INLINE void fworker_fini_thread(fworker* w, const int id)
{
  mv_spin_lock(&w->thread_pool_lock);
  w->thread_pool[w->thread_pool_last++] = &(w->threads[id]);
  mv_spin_unlock(&w->thread_pool_lock);
}

MV_INLINE fthread* fworker_spawn(fworker* w, ffunc f, intptr_t data,
                                 size_t stack_size)
{
  mv_spin_lock(&w->thread_pool_lock);
  fthread* t = w->thread_pool[w->thread_pool_last - 1];
  w->thread_pool_last--;
  mv_spin_unlock(&w->thread_pool_lock);

  // add it to the fthread.
  fthread_create(t, f, data, stack_size);

  // make it schedable.
  fworker_sched_thread(w, t->id);

  return t;
}

MV_INLINE void fworker_work(fworker* w, fthread* f)
{
  if (unlikely(f->state == INVALID)) return;
  tlself.thread = f;

  swap_ctx(&w->ctx, &f->ctx, (intptr_t)f);

  tlself.thread = NULL;
  if (f->state == YIELD)
    fthread_resume(f);
  else if (f->state == INVALID)
    fthread_fini(f);
}

MV_INLINE void fworker_sched_thread(fworker* w, const int id)
{
  sync_set_bit(MOD_POW2(id, WORDSIZE), &w->mask[DIV64(id)]);
#ifdef USE_L1_MASK
  sync_set_bit(DIV512(MOD_POW2(id, 32768)), &w->l1_mask[DIV32768(id)]);
#endif
}

MV_INLINE int pop_work(unsigned long* mask)
{
  int id = find_first_set(*mask);
  *mask = bit_flip(*mask, id);
  return id;
}

#ifdef ENABLE_STEAL
fworker* random_worker();
#endif

#ifndef USE_L1_MASK
MV_INLINE void* wfunc(void* arg)
{
  fworker* w = (fworker*) arg;
  tlself.worker = w;
#ifdef USE_AFFI
  set_me_to(w->id);
#endif

  while (unlikely(!w->stop)) {
#ifdef ENABLE_STEAL
    int has_work = 0;
#endif
    for (int i = 0; i < NMASK; i++) {
      if (w->mask[i] > 0) {
        // Atomic exchange to get the current waiting threads.
        unsigned long local_mask = exchange((unsigned long)0, &(w->mask[i]));
        // Works until it no thread is pending.
        while (likely(local_mask > 0)) {
#ifdef ENABLE_STEAL
          has_work = 1;
#endif
          int id = pop_work(&local_mask);
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
          while (likely(local_mask > 0)) {
            auto id = pop_work(&local_mask);
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
  return 0;
}

#else

MV_INLINE void* wfunc(void* arg)
{
  fworker* w = (fworker*) arg;
  tlself.worker = w;
#ifdef USE_AFFI
  set_me_to(w->id);
#endif

  while (unlikely(!w->stop)) {
    for (int l1i = 0; l1i < 8; l1i++) {
      if (w->l1_mask[l1i] == 0) continue;
      unsigned long local_l1_mask = exchange((unsigned long)0, &(w->l1_mask[l1i]));

      while (local_l1_mask > 0) {
        int ii = find_first_set(local_l1_mask);
        local_l1_mask = bit_flip(local_l1_mask, ii);

        int start_i = MUL8(MUL64(l1i)) + MUL8(ii);
        for (int i = start_i; i < start_i + 8 && i < NMASK; i++) {
          if (w->mask[i] > 0) {
            unsigned long local_mask = 0;
            // Atomic exchange to get the current waiting threads.
            local_mask = exchange(local_mask, &(w->mask[i]));
            // Works until it no thread is pending.
            while (likely(local_mask > 0)) {
              int id = pop_work(&local_mask);
              // Optains the associate thread.
              fthread* f = &w->threads[MUL64(i) + id];
              fworker_work(w, f);
            }
          }
        }
      }
    }
  }

  return 0;
}
#endif  // ifndef L1_MASK

#endif
