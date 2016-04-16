#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdlib.h>

static const int MAX_SEND = 32;  // maximum concurrent send.
static const int MAX_RECV = 32;  // maximum concurrent recv.

#ifndef CONFIG_CONCUR
static const int MAX_CONCURRENCY = MAX_SEND + MAX_RECV;
#else
static const int MAX_CONCURRENCY = CONFIG_CONCUR;
#endif

static const int PACKET_SIZE = (16 * 1024);            // transfer unit size.
static const int SHORT_MSG_SIZE = (PACKET_SIZE - 16);  // short message size.
static const int RNDZ_MSG_SIZE = 48;

static const size_t HEAP_SIZE =
    (size_t)2 * 1024 * 1024 * 1024;  // total pinned heap size.

#endif
