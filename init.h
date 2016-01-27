#ifndef INIT_H_
#define INIT_H_

#include <sys/mman.h>
#include "progress.h"

inline void mpiv_post_recv(mpiv_packet* p) {
    startt(post_timing)
    MPIV.server.post_srq(p);
    stopt(post_timing)
}

inline void MPIV_Init(int &argc, char**& args) {
    MPI_Init(&argc, &args);
    MPIV.tbl.init();
    mpiv_progress_init();
    MPIV.server.init(MPIV.ctx, MPIV.pk_mgr, MPIV.me, MPIV.size);
    MPIV.server.serve();
    MPI_Barrier(MPI_COMM_WORLD);
}

inline void MPIV_Finalize() {
    MPIV.server.finalize();
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}

#endif
