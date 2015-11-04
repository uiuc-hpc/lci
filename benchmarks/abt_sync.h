#ifndef ABT_SYNC_H_
#define ABT_SYNC_H_

struct ABT_sync {
    inline ABT_sync(
        void* buffer_, int size_, int rank_, int tag_) :
        buffer(buffer_), size(size_), rank(rank_), tag(tag_) {

        ABT_cond_create(&cond);
        ABT_mutex_create(&mutex);
        flag = false;
    };

    inline void signal() {
        ABT_mutex_lock(mutex);
        flag = true;
        ABT_cond_signal(cond);
        ABT_mutex_unlock(mutex);
    }

    inline void wait() {
        ABT_mutex_lock(mutex);
        if (!flag) {
            ABT_cond_wait(cond, mutex);
        } 
        ABT_mutex_unlock(mutex);
    }

    ABT_cond cond;
    ABT_mutex mutex;
    bool flag;

    void* buffer;
    int size;
    int rank;
    int tag;
};

#endif
