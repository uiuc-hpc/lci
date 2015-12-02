#ifndef INIT_H_
#define INIT_H_

#include <sys/mman.h>

static void mpiv_progress_init();

inline void mpiv_post_recv(mpiv_packet* p) {
    MPIV.dev_ctx->post_srq_recv((void*) p, (void*) p, sizeof(mpiv_packet), MPIV.sbuf.lkey());
}

inline void MPIV_Init(int &argc, char**& args) {
    MPI_Init(&argc, &args);
    mpiv_tbl_init();
    mpiv_progress_init();
    MPIV.server = new mpiv_server();
    MPIV.server->serve();
    MPI_Finalize();
}

inline void MPIV_Finalize() {
    MPIV.server->finalize();
    delete MPIV.server;
}

#endif
