#ifndef _SYS_UTIME_H
#define _SYS_UTIME_H

#include <sys/cdefs.h>
#include <time.h>

__BEGIN_DECLS

struct utimbuf 
{
  time_t actime;
  time_t modtime; 
};

int utime (const char *, const struct utimbuf *);

__END_DECLS

#endif /* _SYS_UTIME_H */
