#ifndef MPI_FAKE_H
#define MPI_FAKE_H

#include <assert.h>

#define ibv_error_abort(...) {}
#define rdma_num_hcas 1
#define rdma_dreg_cache_limit 0 // TODO(danghvu): check
#define MPI_SUCCESS 0
#define rdma_ndreg_entries 8192
#define MPIU_Free free
#define MPIU_Malloc malloc
#define MPIU_Memset memset
#define MPIU_Assert assert
#define MPIU_Memcpy memcpy
#include <sys/uio.h>


#endif
