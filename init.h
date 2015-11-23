#ifndef INIT_H_
#define INIT_H_

inline void mpiv_post_recv(mpiv_packet* p) {
    MPIV.dev_ctx->post_srq_recv((void*) p,
        &(p->egr.buffer), SHORT, MPIV.sbuf.lkey());
}

inline void MPIV_Init(int &argc, char**& args) {
    MPI_Init(&argc, &args);

    std::vector<rdmax::device> devs = rdmax::device::get_devices();
    assert(devs.size() > 0 && "Unable to find any ibv device");

    MPIV.dev_ctx = new device_ctx(devs.back());
    MPIV.dev_scq = std::move(MPIV.dev_ctx->create_cq(64*1024));
    MPIV.dev_rcq = std::move(MPIV.dev_ctx->create_cq(64*1024));

    // Create RDMA memory.
    int mr_flags =
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    // These are pinned memory.
    void* buf = std::malloc(PACKET_SIZE * NSBUF * 2);
    MPIV.sbuf = std::move(MPIV.dev_ctx->attach_memory(buf, PACKET_SIZE * NSBUF, mr_flags));
    MPIV.sbuf_alloc = new pinned_pool(MPIV.sbuf.ptr());

    MPIV.heap = MPIV.dev_ctx->create_memory((size_t) HEAP_SIZE, mr_flags);
    MPIV.heap_segment = std::move(mbuffer(create_only, MPIV.heap.ptr(), (size_t) HEAP_SIZE));

    // Initialize connection.
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPIV.me = rank;

    for (int i = 0; i < size; i++) {
        MPIV.conn.emplace_back(&MPIV.dev_scq, &MPIV.dev_rcq, MPIV.dev_ctx, &MPIV.sbuf, i);
    }

    /* PREPOST recv and allocate send queue. */
    for (int i = 0; i < NPREPOST; i++) {
        MPIV.prepost.emplace_back((mpiv_packet*) MPIV.sbuf_alloc->allocate());
        MPIV.prepost[i]->header.type = RECV_SHORT;
        mpiv_post_recv(MPIV.prepost[i]);
    }

    for (int i = 0; i < NSBUF - NPREPOST; i++) {
        mpiv_packet* packet = (mpiv_packet*) MPIV.sbuf_alloc->allocate();
        packet->header.type = SEND_SHORT;
        packet->header.from = rank;
        assert(MPIV.squeue.push(packet));
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

inline void MPIV_Finalize() {
    MPI_Finalize();
}

#endif
