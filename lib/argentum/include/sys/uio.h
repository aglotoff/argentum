#ifndef _SYS_UIO_H
#define _SYS_UIO_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

struct iovec {
  void   *iov_base;
  size_t  iov_len;
};

__END_DECLS

#endif /* _SYS_UIO_H */
