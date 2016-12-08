#ifndef CONFIG_H_
#define CONFIG_H_

// Communication setup...
#define MAX_SEND 192  // maximum concurrent send.
#define MAX_RECV 64 // maximum concurrent recv.
#define PACKET_SIZE (16 * 1024 + 64)       // transfer unit size.
#define SHORT_MSG_SIZE (PACKET_SIZE - 16)  // short message size.

#endif
