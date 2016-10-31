#ifndef UTIL_H_
#define UTIL_H_

inline int opal_cube_dim(int value) {
  int dim, size;

  for (dim = 0, size = 1; size < value; ++dim, size <<= 1) {
    continue;
  }

  return dim;
}

inline int opal_hibit(int value, int start) {
  unsigned int mask;

  --start;
  mask = 1 << start;

  for (; start >= 0; --start, mask >>= 1) {
    if (value & mask) {
      break;
    }
  }

  return start;
}

#endif
