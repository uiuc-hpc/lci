#ifndef FULT_INL_H_
#define FULT_INL_H_

#define MUL64(x) ((x)<<6)
#define DIV64(x) ((x)>>6)
#define DIV512(x) ((x)>>9)
#define DIV32768(x) ((x)>>15)
#define MUL8(x) ((x)<<3)
#define MOD_POW2(x, y) ((x) & ((y)-1))

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

inline void fult::resume(bool&) {
  origin_->schedule(id_);
}

inline void fult::join() {
  while (state_ != INVALID) { __fulting->yield(); }
}

inline int fult::get_worker_id() { return origin_->id_; }

inline void fult::start() {
  myfunc_(data_);
  state_ = INVALID;
  ctx_.ret();
}

void fwrapper(intptr_t args) {
  fult* ff = (fult*) args;
  ff->start();
}

inline fworker::fworker() {
  stop_ = true;
  posix_memalign((void**) &lwt_, 64, sizeof(fult) * NMASK * WORDSIZE);
  for (int i = 0; i < NMASK; i++) mask_[i] = 0;
#ifdef USE_L1_MASK
  for (int i = 0; i < 8; i++) l1_mask[i] = 0;
#endif
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

fworker::~fworker() {
  free((void*) lwt_);
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
  // printf("%p swapping %p\n", this, f);
  ctx_.swap(f->ctx(), (intptr_t)f);
  // printf("%p returning from %p\n", this, f);
  __fulting = NULL;
}

inline void fworker::schedule(const int id) {
  sync_set_bit(MOD_POW2(id, WORDSIZE), &mask_[DIV64(id)]);
#ifdef USE_L1_MASK
  sync_set_bit(DIV512(MOD_POW2(id, 32768)), &l1_mask[DIV32768(id)]);
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

fworker* random_worker();

#ifndef USE_L1_MASK
inline void fworker::wfunc(fworker* w) {
  w->id_ = nfworker_.fetch_add(1);
  __wid = w->id_;
#ifdef USE_AFFI
  affinity::set_me_to(w->id_);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop_)) {
    bool has_work = false;
    for (auto i = 0; i < NMASK; i++) {
      auto& mask = w->mask_[i];
      if (mask > 0) {
        // Atomic exchange to get the current waiting threads.
        auto local_mask = exchange((unsigned long)0, &(mask));
        // Works until it no thread is pending.
        while (xlikely(local_mask > 0)) {
          has_work = true;
          auto id = find_first_set(local_mask);
          bit_flip(local_mask, id);
          // Optains the associate thread.
          fult* f = &(w->lwt_[MUL64(i) + id]);
          // Works on it only if it's not completed.
          if (xlikely(f->state_ != INVALID)) {
            w->work(f);
            if (f->state_ == YIELD) w->schedule(f->id_);
            else if (f->state_ == INVALID) w->fin(f->id_);
          }
        }
      }
    }

// #define ENABLE_STEAL
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
              auto id = find_first_set(local_mask);
              bit_flip(local_mask, id);
              // Optains the associate thread.
              fult* f = &(steal->lwt_[MUL64(i) + id]);
              // Works on it only if it's not completed.
              if (xlikely(f->state_ != INVALID)) {
                w->work(f);
                if (f->state_ == YIELD) steal->schedule(f->id_);
                else if (f->state_ == INVALID) steal->fin(f->id_);
              }
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
  __wid = w->id_;
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
            // if (w->id() == 1 && i == 0) printf("%d (%p) update %lx\n", w->id(), w, w->mask_[0]);
            // Works until it no thread is pending.
            while (xlikely(local_mask > 0)) {
              auto id = find_first_set(local_mask);
              bit_flip(local_mask, id);
              // Optains the associate thread.
              fult* f = &w->lwt_[MUL64(i) + id];
              // Works on it only if it's not completed.
              if (xlikely(f->state_ != INVALID)) {
                w->work(f);
                if (f->state_ == YIELD) w->schedule(f->id_);
                else if (f->state_ == INVALID) w->fin(f->id_);
              }
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
#endif // ifndef L1_MASK

#endif
