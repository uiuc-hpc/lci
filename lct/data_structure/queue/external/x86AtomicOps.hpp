#pragma once

#define __CAS2(ptr, o1, o2, n1, n2)                                     \
  ({                                                                    \
    char __ret;                                                         \
    __typeof__(o2) __junk;                                              \
    __typeof__(*(ptr)) __old1 = (o1);                                   \
    __typeof__(o2) __old2 = (o2);                                       \
    __typeof__(*(ptr)) __new1 = (n1);                                   \
    __typeof__(o2) __new2 = (n2);                                       \
    asm volatile("lock cmpxchg16b %2;setz %1"                           \
                 : "=d"(__junk), "=a"(__ret), "+m"(*ptr)                \
                 : "b"(__new1), "c"(__new2), "a"(__old1), "d"(__old2)); \
    __ret;                                                              \
  })

#define CAS2(ptr, o1, o2, n1, n2) __CAS2(ptr, o1, o2, n1, n2)

#define BIT_TEST_AND_SET63(ptr)                \
  ({                                           \
    char __ret;                                \
    asm volatile("lock btsq $63, %0; setnc %1" \
                 : "+m"(*ptr), "=a"(__ret)     \
                 :                             \
                 : "cc");                      \
    __ret;                                     \
  })
