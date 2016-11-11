#ifndef FULT_INL_H_
#define FULT_INL_H_

// Fthread.

#define MUL64(x) ((x) << 6)
#define DIV64(x) ((x) >> 6)
#define DIV512(x) ((x) >> 9)
#define DIV32768(x) ((x) >> 15)
#define MUL8(x) ((x) << 3)
#define MOD_POW2(x, y) ((x) & ((y)-1))

inline void fthread::init(ffunc myfunc, intptr_t data, size_t stack_size) {
  if (stack.sp == NULL) {
    fthread_stack.allocate(stack, stack_size);
  }
  myfunc_ = myfunc;
  data_ = data;
  ctx_.myctx_ = make_fcontext(stack.sp, stack.size, fwrapper);
  state_ = CREATED;
}

inline void fthread::yield() {
  state_ = YIELD;
  ctx_.swap_ctx_parent();
}

inline void fthread::wait() {
  state_ = BLOCKED;
  ctx_.swap_ctx_parent();
}

inline void fthread::resume() { origin_->schedule(id_); }
inline void fthread::fin() { origin_->fin(id_); }

inline void fthread::join() {
  while (state_ != INVALID) {
    tlself.thread->yield();
  }
}

int fthread::get_worker_id() { return origin_->id_; }

inline void fthread::start() {
  myfunc_(data_);
  state_ = INVALID;
  ctx_.swap_ctx_parent();
}

inline void fwrapper(intptr_t args) {
  fthread* ff = (fthread*)args;
  ff->start();
}

/// Fworker.

inline fworker::fworker() {
  stop_ = true;
  posix_memalign((void**)&thread_, 64, sizeof(fthread) * NMASK * WORDSIZE);
  for (int i = 0; i < NMASK; i++) mask_[i] = 0;
#ifdef USE_L1_MASK
  for (int i = 0; i < 8; i++) l1_mask[i] = 0;
#endif
  // Add all free slot.
  memset(thread_, 0, sizeof(fthread) * (NMASK * WORDSIZE));
  for (int i = (int)(NMASK * WORDSIZE) - 1; i >= 0; i--) {
    thread_[i].origin_ = this;
    thread_[i].id_ = i;
    thread_pool_.push(&thread_[i]);
  }
  thread_pool_lock_.clear();
}

inline fworker::~fworker() { free((void*)thread_); }

inline void fworker::fin(int id) {
  SPIN_LOCK(thread_pool_lock_);
  thread_pool_.push(&thread_[id]);
  SPIN_UNLOCK(thread_pool_lock_);
}

fthread* fworker::spawn(ffunc f, intptr_t data, size_t stack_size) {
  SPIN_LOCK(thread_pool_lock_);
  if (thread_pool_.empty()) {
    throw std::runtime_error("Too many threads are spawn");
  }
  fthread* t = thread_pool_.top();
  thread_pool_.pop();
  SPIN_UNLOCK(thread_pool_lock_);
  return fthread_new(t->id(), f, data, stack_size);
}

inline void fworker::work(fthread* f) {
  if (xunlikely(f->state_ == INVALID)) return;
  tlself.thread = f;
  ctx_.swap_ctx(f->ctx(), (intptr_t)f);
  tlself.thread = NULL;
  if (f->state_ == YIELD)
    f->resume();
  else if (f->state_ == INVALID)
    f->fin();
}

inline void fworker::schedule(const int id) {
  sync_set_bit(MOD_POW2(id, WORDSIZE), &mask_[DIV64(id)]);
#ifdef USE_L1_MASK
  sync_set_bit(DIV512(MOD_POW2(id, 32768)), &l1_mask[DIV32768(id)]);
#endif
}

fthread* fworker::fthread_new(const int id, ffunc f, intptr_t data,
                                size_t stack_size) {
  // add it to the fthread.
  thread_[id].init(f, data, stack_size);

  // make it schedable.
  schedule(id);

  return (fthread*)&thread_[id];
}

static inline int pop_work(unsigned long& mask) {
  auto id = find_first_set(mask);
  bit_flip(mask, id);
  return id;
}

#ifdef ENABLE_STEAL
fworker * random_worker();
#endif

#ifndef USE_L1_MASK
inline void fworker::wfunc(fworker* w) {
  w->id_ = nfworker_.fetch_add(1);
  tlself.worker = w;
#ifdef USE_AFFI
  affinity::set_me_to(w->id_);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop_)) {
#ifdef ENABLE_STEAL
    bool has_work = false;
#endif
    for (auto i = 0; i < NMASK; i++) {
      auto& mask = w->mask_[i];
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
          fthread* f = &(w->thread_[MUL64(i) + id]);
          w->work(f);
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
            fthread* f = &(steal->thread_[MUL64(i) + id]);
            w->work(f);
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

inline void fworker::wfunc(fworker* w) {
  w->id_ = nfworker_.fetch_add(1);
  tlself.worker = w;
#ifdef USE_AFFI
  affinity::set_me_to(w->id_);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop_)) {
    for (int l1i = 0; l1i < 8; l1i++) {
      if (w->l1_mask[l1i] == 0) continue;
      auto local_l1_mask = exchange((unsigned long)0, &(w->l1_mask[l1i]));

      while (local_l1_mask > 0) {
        auto ii = find_first_set(local_l1_mask);
        bit_flip(local_l1_mask, ii);

        auto start_i = MUL8(MUL64(l1i)) + MUL8(ii);
        for (auto i = start_i; i < start_i + 8 && i < NMASK; i++) {
          auto& mask = w->mask_[i];
          if (mask > 0) {
            unsigned long local_mask = 0;
            // Atomic exchange to get the current waiting threads.
            local_mask = exchange(local_mask, &(w->mask_[i]));
            // Works until it no thread is pending.
            while (xlikely(local_mask > 0)) {
              auto id = pop_work(local_mask);
              // Optains the associate thread.
              fthread* f = &w->thread_[MUL64(i) + id];
              w->work(f);
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
