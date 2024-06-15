#ifndef _MNTENT_H
# define _MNTENT_H

#include <sys/cdefs.h>
#include <stdio.h>

__BEGIN_DECLS

#define MOUNTED   "/etc/mtab"

struct mntent {
  char    *mnt_fsname;
  char    *mnt_dir;
  char    *mnt_type;
  char    *mnt_opts;
};

struct mntent *getmntent(FILE *filep);

__END_DECLS

#endif
