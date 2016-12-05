#include "mpiv.h"

mv_engine* mv_hdl;
uintptr_t MPIV_HEAP;

__thread int8_t tls_pool_struct[MAX_LOCAL_POOL] = {0};
int mv_pool_nkey = 0;

int PROTO_SHORT;
int PROTO_RECV_READY;
int PROTO_READY_FIN;
int PROTO_AM;
int PROTO_SEND_WRITE_FIN = 99;
