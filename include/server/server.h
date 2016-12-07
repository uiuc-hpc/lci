#ifndef SERVER_H_
#define SERVER_H_

#include "config.h"

#include "affinity.h"
#include "profiler.h"

/*
#ifdef MV_USE_SERVER_OFI
#include "server_ofi.h"
#endif

#ifdef MV_USE_SERVER_RDMAX
#include "server_rdmax.h"
#endif
*/
#include "server_ibv.h"

#endif
