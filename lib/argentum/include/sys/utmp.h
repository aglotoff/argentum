#ifndef _UTMP_H
# define _UTMP_H

#include <sys/cdefs.h>

__BEGIN_DECLS

#define UTMP_FILE     "/etc/utmp"
#define WTMP_FILE     "/etc/wtmp"

#define UT_NAMESIZE   8
#define UT_LINESIZE   8
#define	UT_HOSTSIZE	  16

#define EMPTY         0
#define RUN_LVL       1
#define BOOT_TIME     2
#define NEW_TIME      3
#define OLD_TIME      4
#define INIT_PROCESS  5
#define LOGIN_PROCESS 6
#define USER_PROCESS  7
#define DEAD_PROCESS  8
#define ACCOUNTING    9

struct utmp {
  char  ut_line[UT_LINESIZE];
  char  ut_user[UT_NAMESIZE];
  char	ut_host[UT_HOSTSIZE];
  long  ut_time;
  short ut_type;
  char  ut_id[4];
};

#define ut_name ut_user

__END_DECLS

#endif	/* _UTMP_H */
