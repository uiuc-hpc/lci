#ifndef _FULT_H_
#define _FULT_H_

#include <atomic>
#include <boost/context/all.hpp>
#include <boost/coroutine/standard_stack_allocator.hpp>
#include "bitops.h"

#define DEBUG(x)

#define xlikely(x)       __builtin_expect((x),1)
#define xunlikely(x)     __builtin_expect((x),0)

#define fult_yield() {\
    ((fult*) t_ctx)->yield();\
}

#define fult_wait() { ((fult*)t_ctx)->wait(); }

#define STACK_SIZE (2*8192)
#define NMASK 1
#define WORDSIZE (8 * sizeof(long))

#define MEMFENCE asm volatile ("" : : : "memory")

using boost::context::fcontext_t;
using boost::context::make_fcontext;
using boost::context::jump_fcontext;
using boost::coroutines::standard_stack_allocator;
using boost::coroutines::stack_context;

typedef void(*ffunc)(intptr_t);
static void fwrapper(intptr_t);

class fctx;

thread_local fctx* t_ctx = NULL;
thread_local int myid;

class fctx {
 public:
    inline void swap(fctx* to) {
        t_ctx = to;
        to->parent_ = this;
        DEBUG( printf("%p --> %p\n", this, to); )
        jump_fcontext(&(this->myctx_), to->myctx_, (intptr_t) to);
    }

    inline void swapret() {
        t_ctx = parent_;
        DEBUG( printf("%p --> %p\n", this, parent_); )
        jump_fcontext(&(this->myctx_), parent_->myctx_, (intptr_t) parent_);
    }

    inline fctx* parent() {
        return parent_;
    }

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

static standard_stack_allocator allocator;

class fult : public fctx {
 public:
    fult() : state_(INVALID) {
    }

    inline void set(ffunc myfunc, intptr_t data) {
        allocator.allocate(stack, STACK_SIZE);
        myfunc_ = myfunc;
        data_ = data;
        state_ = CREATED;
        myctx_ = make_fcontext(stack.sp, stack.size, fwrapper);
    }

    inline void yield() {
        state_ = YIELD;
        this->swapret();
    }

    inline void wait() {
        state_ = BLOCKED;
        this->swapret();
    }

    inline void run() {
        (*this->myfunc_)(this->data_);
        // when finish, needs to swap back to the parent.
        this->state_ = INVALID;
        this->swapret();
    }

    inline void set_state(fult_state s) {
        state_ = s;
    }

    inline fult_state state() {
        return state_;
    }

    using fctx::swap;

 private:
    stack_context stack;
    ffunc myfunc_;
    intptr_t data_;
    volatile fult_state state_;
    int8_t __pad__[8];
};

static void fwrapper(intptr_t f) {
    fult* ff = (fult*) f;
    ff->run();
}

class worker : fctx {
 public:
    worker() {
        stop_ = false;
        for (int i = 0 ; i < NMASK; i++) mask_[i] = 0;
    }

    inline void fult_new(const int id, ffunc f, intptr_t data) {
        // add it to the fult.
        lwt_[id].set(f, data);
        // make it schedable.
        DEBUG( printf("new %d\n", id); )
        schedule(id);
    }

    inline void fult_join(int id) {
        while (lwt_[id].state() != INVALID) {
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
        }
    }

    inline void set_state(const int id, fult_state state) {
        lwt_[id].set_state(state);
    }

    inline void schedule(const int id) {
        sync_set_bit(id & (WORDSIZE - 1), &mask_[id / WORDSIZE]);
    }

    inline void deschedule(const int id) {
        sync_clear_bit(id & (WORDSIZE - 1), &mask_[id / WORDSIZE]);
    }

    inline void start() {
        w_ = std::thread(wfunc, this);
    }

    inline void stop() {
        stop_ = true;
        w_.join();
    }

    inline bool is_stop() {
        return stop_;
    }

    static void wfunc(worker*);

 private:
    volatile unsigned long mask_[NMASK];
    volatile bool stop_;
    fult lwt_[WORDSIZE * NMASK];
    std::thread w_;
};

#define SYNC_THRESHOLD 255

#define doschedule(i) { \
    myid = i * WORDSIZE + id;\
    if (w->lwt_[myid].state() != INVALID) w->swap(&w->lwt_[myid]);\
    if (w->lwt_[myid].state() == YIELD) w->schedule(myid);\
}\

#define loop_sched_all(mask, i) {\
    register unsigned long local = exchange((unsigned long) 0, &(mask));\
    while (local > 0) {\
        int id = find_first_set(local);\
        local ^= ((unsigned long)1 << id);\
        doschedule(i);\
    }\
}

void worker::wfunc(worker* w) {
    uint8_t count = 0;
    volatile unsigned long &mask = w->mask_[0];

    while (xunlikely(!w->is_stop())) {
        while (count++ != 0) {
            if (mask > 0) {
                loop_sched_all(mask, 0);
            }
        }
    }
}

class fult_sync {
 public:
    inline fult_sync(
        void* buffer_, int size_, int rank_, int tag_) :
        buffer(buffer_), size(size_), rank(rank_), tag(tag_) {

        id_ = myid;
        ctx_ = static_cast<fult*>(t_ctx);
        parent_ = reinterpret_cast<worker*>(ctx_->parent());
    };

    inline void wait() {
        ctx_->wait();
    }

    inline void signal() {
        parent_->schedule(id_);
    }

    void* buffer;
    int size;
    int rank;
    int tag;

 private:
    fult* ctx_;
    worker* parent_;
    int id_;
};
#endif
