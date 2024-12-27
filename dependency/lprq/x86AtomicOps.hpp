#pragma once

#define BIT_TEST_AND_SET63(ptr)                \
  ({                                           \
    char __ret;                                \
    asm volatile("lock btsq $63, %0; setnc %1" \
                 : "+m"(*ptr), "=a"(__ret)     \
                 :                             \
                 : "cc");                      \
    __ret;                                     \
  })
