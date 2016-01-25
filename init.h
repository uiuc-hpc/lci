#ifndef INIT_H_
#define INIT_H_

#include <sys/mman.h>

static void mpiv_progress_init();

inline void mpiv_post_recv(mpiv_packet* p) {
    startt(post_timing)
    MPIV.server->post_srq(p);
    stopt(post_timing)
}

inline void MPIV_Init(int &argc, char**& args) {
    MPI_Init(&argc, &args);
    MPIV.tbl.init();
    mpiv_progress_init();
    MPIV.server = new mpiv_server();
    MPIV.server->init();
    MPIV.server->serve();
    MPI_Barrier(MPI_COMM_WORLD);
}

inline void MPIV_Finalize() {
    MPIV.server->finalize();
    delete MPIV.server;
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}

#endif
