#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdlib.h>

// Communication setup...
#define MAX_SEND 224  // maximum concurrent send.
#define MAX_RECV 32   // maximum concurrent recv.
#define MAX_CONCURRENCY (MAX_SEND + MAX_RECV)
#define PACKET_SIZE (16 * 1024 + 64)       // transfer unit size.
#define SHORT_MSG_SIZE (PACKET_SIZE - 16)  // short message size.

/*! Need to define server type. */
#define MV_USE_SERVER_RDMAX
// #define MV_USE_SERVER_OFI

#endif
