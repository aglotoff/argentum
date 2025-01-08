#ifndef _SYS_PARAM_H
# define _SYS_PARAM_H

#include <sys/config.h>
#include <sys/syslimits.h>
#include <machine/endian.h>
#include <machine/param.h>

#ifndef NBBY
# define NBBY 8		/* number of bits in a byte */
#endif
#ifndef HZ
# define HZ (60)
#endif
#ifndef NOFILE
# define NOFILE	(32)
#endif
#ifndef PATHSIZE
# define PATHSIZE (1024)
#endif

#define MAXPATHLEN PATH_MAX

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#ifndef howmany
#define    howmany(x, y)   (((x)+((y)-1))/(y))
#endif

#endif
