#ifndef __DIRENT_H__
#define __DIRENT_H__

#include <stdint.h>
#include <sys/types.h>

typedef int DIR;

struct dirent {
  ino_t     d_ino;
  off_t     d_off;
  uint16_t  d_reclen;
  uint8_t   d_type;
  uint16_t  d_namelen;
  char      d_name[0];
};

int            closedir(DIR *);
ssize_t        getdents(int, void *, size_t);
DIR           *opendir(const char *);
struct dirent *readdir(DIR *);

#endif  // !__DIRENT_H__
