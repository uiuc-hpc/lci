/* CPU() - the target CPU architecture */
#define CPU(WTF_FEATURE) (defined WTF_CPU_##WTF_FEATURE  && WTF_CPU_##WTF_FEATURE)

#define WTF_CPU_X86_64 1
#define WTF_CPU_64BIT 1
