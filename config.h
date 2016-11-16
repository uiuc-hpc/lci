#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdlib.h>

// Communication setup...
static const int MAX_SEND = 64;  // maximum concurrent send.
static const int MAX_RECV = 32;  // maximum concurrent recv.
static const int MAX_CONCURRENCY = MAX_SEND + MAX_RECV;
static const int PACKET_SIZE = (16 * 1024 + 64);       // transfer unit size.
static const int SHORT_MSG_SIZE = (PACKET_SIZE - 16);  // short message size.
static const int RNDZ_MSG_SIZE = 48; // control message.

static const size_t HEAP_SIZE =
    (size_t)2 * 1024 * 1024 * 1024;  // total pinned heap size.

class ServerOFI;
class ServerRdmax;

using Server = ServerRdmax;

#define HASH_ARR
#define PP_NS

#endif
