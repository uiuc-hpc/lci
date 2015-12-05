#ifndef INIT_H_
#define INIT_H_

#include <sys/mman.h>

static void mpiv_progress_init();

inline void mpiv_post_recv(mpiv_packet* p) {
    startt(post_timing)
    MPIV.dev_ctx->post_srq_recv((void*) p, (void*) p, sizeof(mpiv_packet), MPIV.sbuf.lkey());
    stopt(post_timing)
}

inline void MPIV_Init(int &argc, char**& args) {
    MPI_Init(&argc, &args);
    mpiv_tbl_init();
    mpiv_progress_init();
    MPIV.server = new mpiv_server();
    MPIV.server->init();
    MPIV.server->serve();
    MPI_Barrier(MPI_COMM_WORLD);
}

inline void MPIV_Finalize() {
    MPI_Barrier(MPI_COMM_WORLD);
    MPIV.server->finalize();
    delete MPIV.server;
    MPI_Finalize();
}

#endif
