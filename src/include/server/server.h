#ifndef SERVER_H_
#define SERVER_H_

#ifdef LC_USE_SERVER_OFI
#include "server_ofi.h"
#endif

#ifdef LC_USE_SERVER_IBV
#include "server_ibv.h"
#endif

#ifdef LC_USE_SERVER_PSM
#include "server_psm2.h"
#endif

#endif
