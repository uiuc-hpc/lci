#ifndef SERVER_H_
#define SERVER_H_

#ifdef MV_USE_SERVER_OFI
#define server_context 
#include "server_ofi.h"
#endif

#ifdef MV_USE_SERVER_IBV
#define server_context fi_context
#include "server_ibv.h"
#endif

#endif
