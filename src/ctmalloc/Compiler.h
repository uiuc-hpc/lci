/* COMPILER() - the compiler being used to build the project */
#define COMPILER(WTF_FEATURE) (defined WTF_COMPILER_##WTF_FEATURE  && WTF_COMPILER_##WTF_FEATURE)

#define WTF_COMPILER_GCC 1

#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#define NEVER_INLINE __attribute__((__noinline__))

#define LIKELY(x) __builtin_expect((x), 1)
#define UNLIKELY(x) __builtin_expect((x), 0)
