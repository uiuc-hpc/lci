#ifndef MPIX_H_
#define MPIX_H_

#include <libcuckoo/cuckoohash_map.hh>

cuckoohash_map<int, void*> tbl;

void MPIX_Recv(void* buffer, int size, int rank, int tag) {
    MPIX_Request s(buffer, size, rank, tag);
    if (!tbl.insert(tag, (void*) &s)) {
        void* buf = tbl[tag];
        memcpy(buffer, buf, size);
        free(buf);
        tbl.erase(tag);
    } else {
        s.wait();
    }
}

inline void MPIX_Progress() {
    MPI_Status stat;
    int flag = 0;
    MPI_Iprobe(1, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);

    // nothing.
    if (!flag) return;

    int tag = stat.MPI_TAG;
    void *sync = NULL;
    if (!tbl.find(tag, sync)) {
        // unmatched recv, allocate a buffer for it and continue.
        int size = 0;
        MPI_Get_count(&stat, MPI_BYTE, &size);
        void* buf = std::malloc(size);
        MPI_Recv(buf, size, MPI_BYTE, stat.MPI_SOURCE, tag, 
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (!tbl.insert(tag, buf)) {
            MPIX_Request* fsync = (MPIX_Request*) (void*) tbl[tag];
            memcpy(fsync->buffer, buf, size);
            fsync->signal();
            free(buf);
            tbl.erase(tag);
        } else {
            // this branch is handle
            // by the thread.
        }
    } else {
        MPIX_Request* fsync = (MPIX_Request*) sync;
        MPI_Recv(fsync->buffer, fsync->size, MPI_BYTE, 1, tag, 
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        fsync->signal();
        tbl.erase(tag);
    }
}

#endif
