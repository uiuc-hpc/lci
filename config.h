#ifndef CONFIG_H_
#define CONFIG_H_

#define PACKET_SIZE (16*1024)
#define SHORT (PACKET_SIZE - sizeof(mpiv_packet_header))
#define INLINE 512

#define NSBUF 32
#define NPREPOST 16

#define HEAP_SIZE 64*1024*1024

/** hash_table */
#define USE_COCK

// #define USE_LF

/** profiling */

// #define USE_TIMING 

// #define USE_PAPI
#endif
