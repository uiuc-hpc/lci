#ifndef LC_DEBUG_H_
#define LC_DEBUG_H_

#ifdef LC_DEBUG
#define dprintf printf
#else
#define dprintf(...) 
#endif

#endif
