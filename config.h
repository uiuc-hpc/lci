#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdlib.h>

// Communication setup...
#define MAX_SEND 224 // maximum concurrent send.
#define MAX_RECV 32  // maximum concurrent recv.
#define MAX_CONCURRENCY (MAX_SEND+MAX_RECV)
#define PACKET_SIZE (16 * 1024 + 64)       // transfer unit size.
#define SHORT_MSG_SIZE (PACKET_SIZE - 16)  // short message size.

#define HEAP_SIZE ((size_t)2 * 1024 * 1024 * 1024)  // total pinned heap size.

#ifdef __cplusplus
class ServerOFI;
class ServerRdmax;
using Server = ServerOFI;
#endif

#endif
