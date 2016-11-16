#ifndef FULT_CONFIG_H_
#define FULT_CONFIG_H_

#ifndef USE_L1_MASK
constexpr int NMASK = 8;
#else
constexpr int NMASK = 8 * 8 * 64;
#endif

constexpr int WORDSIZE = (8 * sizeof(long));
constexpr int F_STACK_SIZE = 64 * 1024;
constexpr int MAIN_STACK_SIZE = 1024 * 1024;

// #define ENABLE_STEAL

#endif
