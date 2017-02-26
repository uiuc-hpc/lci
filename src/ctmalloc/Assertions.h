#include "wtf/Compiler.h"

#define IMMEDIATE_CRASH() __builtin_trap()

#define ASSERT(assertion)

#define RELEASE_ASSERT(assertion) (UNLIKELY(!(assertion)) ? (IMMEDIATE_CRASH()) : (void)0)

#define COMPILE_ASSERT(exp, name) typedef int dummy##name [(exp) ? 1 : -1]
