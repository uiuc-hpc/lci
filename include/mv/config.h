#ifndef CONFIG_H_
#define CONFIG_H_

// Communication setup...
#ifdef MV_USE_SERVER_OFI
#define MAX_PACKET 1024
#else
#define MAX_PACKET 256
#endif

#define MAX_RECV 64
#define MAX_SEND (MAX_PACKET - MAX_RECV) // maximum concurrent send.
#define PACKET_SIZE (16 * 1024 + 64)       // transfer unit size.
#define SHORT_MSG_SIZE (PACKET_SIZE - sizeof(packet_header))  // short message size.

#endif
