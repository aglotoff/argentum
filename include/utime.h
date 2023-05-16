#ifndef __UTIME_H__
#define __UTIME_H__

#include <sys/types.h>

struct utimbuf {
  time_t    actime;    // Access time. 
  time_t    modtime;   // Modification time. 
};

int utime(const char *, const struct utimbuf *);

#endif  // !__UTIME_H__
