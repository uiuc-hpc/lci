#include "mpiv.h"

mv_engine* mv_hdl;
mbuffer heap_segment;
int PROTO_SHORT;
int PROTO_RECV_READY;
int PROTO_READY_FIN;
int PROTO_AM;
int PROTO_SEND_WRITE_FIN = 99;
