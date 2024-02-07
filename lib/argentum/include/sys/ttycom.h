#ifndef _SYS_TTYCOM_H
#define _SYS_TTYCOM_H

#include <sys/ioccom.h>
#include <sys/types.h>

#define TCGETPGRP   _IOR('t', 0, pid_t)
#define TCSETPGRP   _IOW('t', 1, pid_t)

#endif
