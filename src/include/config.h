#ifndef CONFIG_H_
#define CONFIG_H_

// NOTE(danghvu): These numbers are tweaked for performance and some alignment.
// Update at our own risk.

#ifdef LC_USE_SERVER_OFI
#define MAX_PACKET 1024
#define LC_PACKET_SIZE (32 * 1024 + 4096)
#endif

#ifdef LC_USE_SERVER_PSM
#define MAX_PACKET 1024
#define LC_PACKET_SIZE (32 * 1024 + 4096)
#endif

#ifdef LC_USE_SERVER_IBV
#define MAX_PACKET 256
#define LC_PACKET_SIZE (16 * 1024 + 4096)
#endif

#define MAX_RECV 64
#define MAX_SEND (MAX_PACKET - MAX_RECV)  // maximum concurrent send.

#define SHORT_MSG_SIZE (LC_PACKET_SIZE - sizeof(struct packet_context))

#define POST_MSG_SIZE (SHORT_MSG_SIZE)

#ifdef LC_SERVER_INLINE
#define SERVER_MAX_INLINE 32
#else
#define SERVER_MAX_INLINE 0
#endif

// Using LCRQ or spinlock.
#define USE_CCQ

// Use memory registration (Must also enable tcmalloc)
// #define USE_DREG

#define THREAD_PER_CORE 1

#endif
