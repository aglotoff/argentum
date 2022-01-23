#ifndef __INCLUDE_DIRENT_H__
#define __INCLUDE_DIRENT_H__

#include <stdint.h>
#include <sys/types.h>

struct dirent {
  ino_t     d_ino;
  off_t     d_off;
  uint16_t  d_reclen;
  uint8_t   d_type;
  uint16_t  d_namelen;
  char      d_name[0];
};

ssize_t getdents(int, void *, size_t);

#endif  // !__INCLUDE_DIRENT_H__
