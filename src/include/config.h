#ifndef CONFIG_H_
#define CONFIG_H_

// NOTE(danghvu): These numbers are tweaked for performance and some alignment.
// Update at our own risk.

#ifdef MV_USE_SERVER_OFI
#define MAX_PACKET 1024
#define MV_PACKET_SIZE (64 * 1024 + 4096)
#else
#define MAX_PACKET 256
#define MV_PACKET_SIZE (16 * 1024 + 4096)
#endif

#define MAX_RECV 64
#define MAX_SEND (MAX_PACKET - MAX_RECV)  // maximum concurrent send.

#define SHORT_MSG_SIZE \
  (MV_PACKET_SIZE - sizeof(struct packet_header) - sizeof(struct packet_context))

#define POST_MSG_SIZE \
  (SHORT_MSG_SIZE + sizeof(struct packet_header))

#define MAX_COMM_ID 4096

#define SERVER_MAX_INLINE (sizeof(struct packet_header) + 16)

// Using LCRQ or spinlock.
#define USE_CCQ

#endif
