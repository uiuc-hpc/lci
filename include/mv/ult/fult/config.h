#ifndef FULT_CONFIG_H_
#define FULT_CONFIG_H_

#ifndef USE_L1_MASK
#define NMASK 8
#else
#define NMASK (8 * 8 * 64)
#endif

#define WORDSIZE (8 * sizeof(long))
#define F_STACK_SIZE (16 * 1024)
#define MAIN_STACK_SIZE  (1024 * 1024)

// #define ENABLE_STEAL

#endif
