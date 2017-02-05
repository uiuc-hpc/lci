#ifndef CONFIG_H_
#define CONFIG_H_

// Communication setup...
#ifdef MV_USE_SERVER_OFI
#define MAX_PACKET 1024
#define MV_PACKET_SIZE (128 * 1024 + 128)
#else
#define MAX_PACKET 256
#define MV_PACKET_SIZE (16 * 1024 + 64)
#endif

#define MAX_RECV 64
#define MAX_SEND (MAX_PACKET - MAX_RECV)  // maximum concurrent send.
#define SHORT_MSG_SIZE \
  (MV_PACKET_SIZE - sizeof(packet_header))  // short message size.

// Using LCRQ or spinlock.
#define USE_CCQ

#endif
