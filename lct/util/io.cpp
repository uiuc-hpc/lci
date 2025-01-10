#include <sys/param.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include "lcti.hpp"

ssize_t read_file_vararg(char* buffer, size_t max, const char* filename_fmt,
                         va_list ap)
{
  char filename[MAXPATHLEN];
  ssize_t read_bytes;
  int fd;

  memset(buffer, 0, max);

  vsnprintf(filename, MAXPATHLEN, filename_fmt, ap);

  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  read_bytes = read(fd, buffer, max - 1);

  close(fd);
  return read_bytes;
}

ssize_t LCT_read_file(char* buffer, size_t max, const char* filename_fmt, ...)
{
  ssize_t read_bytes;
  va_list ap;

  va_start(ap, filename_fmt);
  read_bytes = read_file_vararg(buffer, max, filename_fmt, ap);
  va_end(ap);

  return read_bytes;
}