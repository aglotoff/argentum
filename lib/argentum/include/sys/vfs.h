#ifndef _SYS_VFS_H
# define _SYS_VFS_H

#include <sys/types.h>

typedef struct fsid {
  int32_t val[2];
} fsid_t;

struct statfs {
  short         f_type;
  fsblkcnt_t    f_blocks;
  fsblkcnt_t    f_bfree;
  unsigned long	f_bsize;
  fsblkcnt_t    f_bavail;
  fsfilcnt_t    f_files;
  fsfilcnt_t    f_ffree;
  fsfilcnt_t    f_favail;
  fsid_t        f_fsid;
};

int statfs(const char *path, struct statfs *buf);

#endif
