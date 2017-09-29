#ifndef FULT_INL_H_
#define FULT_INL_H_

#include <string.h>

#include "lc/affinity.h"
#include "lc/lock.h"
#include "lc/macro.h"

// Fthread.

#define MUL64(x) ((x) << 6)
#define DIV64(x) ((x) >> 6)
#define DIV512(x) ((x) >> 9)
#define DIV32768(x) ((x) >> 15)
#define MUL8(x) ((x) << 3)
#define MOD_POW2(x, y) ((x) & ((y)-1))

#define MAX_THREAD (WORDSIZE * NMASK)
#define MAX_INIT_THREAD (WORDSIZE * 8) 

extern int nfworker_;

LC_INLINE void fthread_create(fthread* f, ffunc func, void* data,
                              size_t stack_size)
{
  if (unlikely(f->stack == NULL) || stack_size > 8192) {
    free(f->stack);
    void* memory = lc_memalign(64, stack_size);
    if (memory == 0) {
      fprintf(stderr, "No more memory for stack\n");
      exit(EXIT_FAILURE);
    }
    f->stack = memory;
  }
  f->waiter = 0;
  f->waiter_lock = LC_SPIN_UNLOCKED;
  f->func = func;
  f->data = data;
  f->ctx.stack_ctx = make_fcontext((void*)((uintptr_t)f->stack + stack_size),
                                   stack_size, fwrapper);
  f->state = CREATED;
}

LC_INLINE void fthread_yield(fthread* f)
{
  f->state = YIELD;
  swap_ctx_parent(&f->ctx);
}

LC_INLINE void fthread_wait(fthread* f)
{
  f->state = BLOCKED;
  swap_ctx_parent(&f->ctx);
}

LC_INLINE void fthread_resume(fthread* f)
{
  fworker_sched_thread(f->origin, f->id);
}

LC_INLINE void fthread_fini(fthread* f)
{
  fworker_fini_thread(f->origin, f->id);
}

LC_INLINE void fthread_join(fthread_t* f)
{
  if (*f == tlself.thread) return;
  int count = 0;
  while (*((volatile fthread_t*)f) != NULL && count < 3) {
    fthread_yield(tlself.thread);
  }
  fthread* ff = *f;
  if (*f != NULL) {
    lc_spin_lock(&ff->waiter_lock);
    while (*((volatile fthread_t*)f) != NULL) {
      ff->waiter = tlself.thread;
      lc_spin_unlock(&ff->waiter_lock);
      fthread_wait(tlself.thread);
      lc_spin_lock(&ff->waiter_lock);
    }
    lc_spin_unlock(&ff->waiter_lock);
  }
}

static void* fwrapper(void* args)
{
  fthread* f = (fthread*)args;
  f->func(f->data);
  lc_spin_lock(&f->waiter_lock);
  *(f->uthread) = NULL;
  f->state = INVALID;
  if (f->waiter) fthread_resume(f->waiter);
  lc_spin_unlock(&f->waiter_lock);
  swap_ctx_parent(&f->ctx);
  return 0;
}

/// Fworker.
LC_INLINE void add_more_threads(fworker* w, int num_threads)
{
  int thread_size = w->thread_size;
  // w->threads = (fthread**) realloc(w->threads, sizeof(uintptr_t) * (thread_size + num_threads));
  w->thread_pool = (fthread**) realloc(w->thread_pool, sizeof(uintptr_t) * (thread_size + num_threads));

  for (int i = 0; i < num_threads; i++) {
    fthread* t = (fthread*) lc_memalign(64, sizeof(struct fthread)); // &(w->threads[thread_size + i]);
    memset(t, 0, sizeof(struct fthread));
    fthread_init(t);
    t->origin = w;
    t->id = thread_size + i;
    w->thread_pool[w->thread_pool_last++] = t;
    w->threads[t->id] = t;
  }
  w->thread_size += num_threads;
}

LC_INLINE void fworker_create(fworker** w_ptr)
{
  fworker* w = (fworker*)lc_memalign(64, sizeof(struct fworker));
  w->stop = 1;
  w->thread_pool_last = 0;
  w->thread_size = 0;
  w->threads = lc_memalign(64, sizeof(uintptr_t) * MAX_THREAD);
  w->thread_pool = NULL;

  for (int i = 0; i < NMASK; i++) w->mask[i] = 0;
#ifdef USE_L1_MASK
  for (int i = 0; i < 8; i++) w->l1_mask[i] = 0;
#endif
  w->thread_pool_lock = LC_SPIN_UNLOCKED;

  lc_spin_lock(&w->thread_pool_lock);
  add_more_threads(w, MAX_INIT_THREAD);
  lc_spin_unlock(&w->thread_pool_lock);

  lc_mem_fence();
  *w_ptr = w;
}

LC_INLINE void fworker_destroy(fworker* w)
{
  for (int i = 0; i < w->thread_size; i++) {
    free(w->thread_pool[i]->stack);
    free(w->thread_pool[i]);
  }
  free((void*)w->thread_pool);
  free(w);
}

LC_INLINE void fworker_fini_thread(fworker* w, const int id)
{
  lc_spin_lock(&w->thread_pool_lock);
  w->thread_pool[w->thread_pool_last++] = w->threads[id];
  lc_spin_unlock(&w->thread_pool_lock);
}

LC_INLINE void fworker_spawn(fworker* w, ffunc f, void* data,
    size_t stack_size, fthread_t* thread)
{
  // Poll abit to wait for some free threads.
  int count = 0;
  while (w->thread_pool_last == 0 && count++ < 255)
    fthread_yield(tlself.thread);

  lc_spin_lock(&w->thread_pool_lock);
  while (w->thread_pool_last == 0) {
    if (w->thread_size < MAX_THREAD)
      add_more_threads(w, MIN(w->thread_size, MAX_THREAD - w->thread_size));
    else {
      lc_spin_unlock(&w->thread_pool_lock);
      while (w->thread_pool_last == 0)
        fthread_yield(tlself.thread);
      lc_spin_lock(&w->thread_pool_lock);
    }
  }
  fthread* t = w->thread_pool[w->thread_pool_last - 1];
  w->thread_pool_last--;
  t->uthread = thread;
  *thread = t;
  lc_spin_unlock(&w->thread_pool_lock);

  // add it to the fthread.
  fthread_create(t, f, data, stack_size);

  // make it schedable.
  fworker_sched_thread(w, t->id);
}

LC_INLINE void fworker_work(fworker* w, fthread* f)
{
  if (unlikely(f->state == INVALID)) return;
  tlself.thread = f;

  swap_ctx(&w->ctx, &f->ctx, f);

  tlself.thread = NULL;
  if (f->state == YIELD)
    fthread_resume(f);
  else if (f->state == INVALID)
    fthread_fini(f);
}

LC_INLINE void fworker_sched_thread(fworker* w, const int id)
{
  sync_set_bit(MOD_POW2(id, WORDSIZE), &w->mask[DIV64(id)]);
#ifdef USE_L1_MASK
  sync_set_bit(DIV512(MOD_POW2(id, 32768)), &w->l1_mask[DIV32768(id)]);
#endif
}

LC_INLINE int pop_work(unsigned long* mask)
{
  int id = find_first_set(*mask);
  *mask = bit_flip(*mask, id);
  return id;
}

#ifdef ENABLE_STEAL
fworker* random_worker();
LC_INLINE void random_steal(fworker* w) {
  fworker* steal = random_worker();
  for (int i = 0; i < NMASK; i++) {
    if (steal->mask[i] > 0) {
      // Atomic exchange to get the current waiting threads.
      unsigned long local_mask = exchange((unsigned long)0, &(steal->mask[i]));
      // Works until it no thread is pending.
      while (likely(local_mask > 0)) {
        int id = pop_work(&local_mask);
        // Optains the associate thread.
        fthread* f = steal->threads[MUL64(i) + id];
        fworker_work(w, f);
      }
      break;
    }
  }
}

#ifdef USE_L1_MASK
LC_INLINE void random_steal_l1(fworker* w) {
  fworker* steal = random_worker();

  for (int l1i = 0; l1i < 8; l1i++) {
    if (steal->l1_mask[l1i] == 0) continue;
    unsigned long local_l1_mask = steal->l1_mask[l1i];

    if (local_l1_mask > 0) {
      int ii = find_first_set(local_l1_mask);
      int start_i = MUL8(MUL64(l1i)) + MUL8(ii);
      for (int i = start_i; i < start_i + 8 && i < NMASK; i++) {
        if (steal->mask[i] > 0) {
          // Atomic exchange to get the current waiting threads.
          unsigned long local_mask = exchange(0, &(steal->mask[i]));
          // Works until it no thread is pending.
          while (likely(local_mask > 0)) {
            int id = pop_work(&local_mask);
            // Optains the associate thread.
            fthread* f = steal->threads[MUL64(i) + id];
            fworker_work(w, f);
          }
          return;
        }
      }
    }
  }
}
#endif
#endif

#if 0
__thread double wtimework, wtimeall;
#endif

#ifndef USE_L1_MASK
LC_INLINE void* wfunc(void* arg)
{
  fworker* w = (fworker*)arg;
  tlself.worker = w;
  set_me_to(w->id);

#ifdef ENABLE_STEAL
  if (w->id == 0) {
    fprintf(stderr, "[FULT] Using steal\n");
  }
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
#ifdef ENABLE_STEAL
        has_work = 1;
#endif
        while (likely(local_mask > 0)) {
          int id = pop_work(&local_mask);
          // Optains the associate thread.
          fthread* f = w->threads[MUL64(i) + id];
          fworker_work(w, f);
        }
      }
    }

#ifdef ENABLE_STEAL
    if (!has_work && nfworker_ > 1)
      random_steal(w);
#endif
  }
  return 0;
}

#else

LC_INLINE void* wfunc(void* arg)
{
  fworker* w = (fworker*)arg;
  tlself.worker = w;
  set_me_to(w->id);

  if (w->id == 0) {
#ifdef ENABLE_STEAL
    fprintf(stderr, "[FULT] Using steal\n");
#endif
    fprintf(stderr, "[FULT] Using L1_MASK\n");
  }
 
  while (unlikely(!w->stop)) {
    int has_work = 0;
    for (int l1i = 0; l1i < 8; l1i++) {
      if (w->l1_mask[l1i] == 0) continue;
      unsigned long local_l1_mask =
          exchange((unsigned long)0, &(w->l1_mask[l1i]));

      while (local_l1_mask > 0) {
        int ii = find_first_set(local_l1_mask);
        local_l1_mask = bit_flip(local_l1_mask, ii);

        int start_i = MUL8(MUL64(l1i)) + MUL8(ii);
        for (int i = start_i; i < start_i + 8 && i < NMASK; i++) {
          if (w->mask[i] > 0) {
            has_work = 1;
            // Atomic exchange to get the current waiting threads.
            unsigned long local_mask = exchange(0, &(w->mask[i]));
            // Works until it no thread is pending.
            while (likely(local_mask > 0)) {
              int id = pop_work(&local_mask);
              // Optains the associate thread.
              fthread* f = w->threads[MUL64(i) + id];
              fworker_work(w, f);
            }
          }
        }
      }
    }

#ifdef ENABLE_STEAL
    if (!has_work && nfworker_ > 1) {
      random_steal_l1(w);
    }
#endif
  }

  return 0;
}
#endif  // ifndef L1_MASK

#endif
