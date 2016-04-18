#ifndef FULT_INL_H_
#define FULT_INL_H_

inline void fult::init(ffunc myfunc, intptr_t data, size_t stack_size) {
  if (stack.sp == NULL) {
    fult_stack.allocate(stack, stack_size);
  }
  myfunc_ = myfunc;
  data_ = data;
  ctx_.myctx_ = make_fcontext(stack.sp, stack.size, fwrapper);
  state_ = CREATED;
}

inline void fult::yield() {
  state_ = YIELD;
  ctx_.ret();
}

inline void fult::wait(bool&) {
  state_ = BLOCKED;
  ctx_.ret();
}

inline void fult::resume(bool&) { origin_->schedule(id_); }

inline void fult::done() { origin_->fin(id_); }

inline void fult::join() {
  while (state_ != INVALID) { __fulting->yield(); }
}

inline void fult::start() {
  (myfunc_)(data_);
  state_ = INVALID;
  ctx_.ret();
}

inline int fult::get_worker_id() { return origin_->id_; }

static void fwrapper(intptr_t args) {
  fult* ff = (fult*)args;
  ff->start();
}

#ifdef USE_WORKER_WAIT
void fworker::wait(bool& flag) {
  std::unique_lock<std::mutex> lk(m_);
  cv_.wait(lk, [&flag] { return flag; });
}

void fworker::resume(bool& flag) {
  {
    std::lock_guard<std::mutex> lk(m_);
    flag = true;
  }
  cv_.notify_one();
}
#endif

inline fworker::fworker() {
  stop_ = true;
  // Reset all mask.
  l1_mask = 0;
  for (int i = 0; i < NMASK; i++) mask_[i] = 0;
  // Add all free slot.
  memset(lwt_, 0, sizeof(fult) * (NMASK * WORDSIZE));
  tid_pool = std::move(std::unique_ptr<boost::lockfree::stack<fult_t>>(
        new boost::lockfree::stack<fult_t>(NMASK * WORDSIZE)));

  for (int i = (int)(NMASK * WORDSIZE) - 1; i >= 0; i--) {
    lwt_[i].origin_ = this;
    lwt_[i].id_ = i;
    tid_pool->push(&lwt_[i]);
  }
}

inline fult_t fworker::spawn(ffunc f, intptr_t data, size_t stack_size) {
  fult_t t;
  if (!tid_pool->pop(t)) {
    throw std::runtime_error("Too many threads are spawn");
  }
  return fult_new(t->id(), f, data, stack_size);
}

inline void fworker::work(fult* f) {
  __fulting = f;
  ctx_.swap(f->ctx(), (intptr_t)f);
  __fulting = NULL;
}

inline void fworker::schedule(const int id) {
  sync_set_bit(id & (WORDSIZE - 1), &mask_[id >> 6]);
#ifdef USE_L1_MASK
  sync_set_bit(id >> 9, &l1_mask);
#endif
}

inline fult_t fworker::fult_new(const int id, ffunc f, intptr_t data,
                        size_t stack_size) {
  // add it to the fult.
  lwt_[id].init(f, data, stack_size);

  // make it schedable.
  schedule(id);

  return (fult_t)&lwt_[id];
}

inline void fworker::wfunc(fworker* w) {
  w->id_ = nfworker_.fetch_add(1);
  bool flag;
#ifdef USE_AFFI
  affinity::set_me_to(w->id_);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop_)) {
#ifdef USE_L1_MASK
    if (w->l1_mask == 0) continue;
    auto local_l1_mask = exchange((unsigned long)0, &(w->l1_mask));
    while (local_l1_mask > 0) {
      auto ii = find_first_set(local_l1_mask);
      local_l1_mask ^= ((unsigned long)1 << ii);

      for (auto i = ii * 8; i < ii * 8 + 8 && i < NMASK; i++) {
#else
    for (auto i = 0; i < NMASK; i++) {
      {
#endif
        auto& mask = w->mask_[i];
        if (mask > 0) {
          // Atomic exchange to get the current waiting threads.
          auto local_mask = exchange((unsigned long)0, &(mask));

          // Works until it no thread is pending.
          while (local_mask > 0) {
            auto id = find_first_set(local_mask);
            // Flips the set bit.
            local_mask ^= ((unsigned long)1 << id);
            // Optains the associate thread.
            fult* f = &w->lwt_[(i << 6) + id];
            // Works on it only if it's not completed.
            if (f->state() != INVALID)  {
              w->work(f);
              // Cleanup after working.
              if (f->state() == YIELD) f->resume(flag);
              else if (f->state() == INVALID) f->done();
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

#endif
