#ifndef __DIRENT_H__
#define __DIRENT_H__

#include <limits.h>
#include <sys/types.h>

struct dirent {
  /** File serial number */
  ino_t     d_ino;
  size_t    d_reclen;
  char      d_name[0];
};

#define _DIRENT_MAX  (sizeof(struct dirent) + NAME_MAX + 1)

typedef struct _Dir {
  int   _Fd;
  char  _Buf[_DIRENT_MAX];
  char *_Buf_end;
  char *_Next;
} DIR;

int            closedir(DIR *);
DIR           *fdopendir(int);
ssize_t        getdents(int, void *, size_t);
DIR           *opendir(const char *);
struct dirent *readdir(DIR *);
void           rewinddir(DIR *);

#endif  // !__DIRENT_H__
