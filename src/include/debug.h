#ifndef LC_DEBUG_H_
#define LC_DEBUG_H_

#include "config.h"

#ifdef LC_SERVER_DEBUG
#define dprintf printf
#else
#define dprintf(...) {};
#endif

#endif
