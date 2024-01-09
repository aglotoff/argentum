#ifndef _SYS_SYSCALL_H
 #define _SYS_SYSCALL_H

#include <errno.h>
#include <stdint.h>

// System call numbers
enum {
  __SYS_TEST = 0,
  __SYS_FORK,
  __SYS_EXEC,
  __SYS_WAIT,
  __SYS_EXIT,
  __SYS_ALARM,
  __SYS_GETPID,
  __SYS_GETPPID,
  __SYS_GETDENTS,
  __SYS_CHDIR,
  __SYS_FCHDIR,
  __SYS_OPEN,
  __SYS_FCNTL,
  __SYS_SEEK,
  __SYS_UMASK,
  __SYS_LINK,
  __SYS_MKNOD,
  __SYS_UNLINK,
  __SYS_RMDIR,
  __SYS_STAT,
  __SYS_CLOSE,
  __SYS_READ,
  __SYS_WRITE,
  __SYS_SBRK,
  __SYS_UNAME,
  __SYS_CHMOD,
  __SYS_FCHMOD,
  __SYS_CLOCK_TIME,
  __SYS_SOCKET,
  __SYS_BIND,
  __SYS_LISTEN,
  __SYS_CONNECT,
  __SYS_ACCEPT,
};

// Generic system call: pass system call number as an immediate operand of the
// SVC instruction, and up to three parameters in R0, R1, R2.
// Interrupt kernel with the SVC instruction.
//
// If a negative value is returned, set the errno variable and return -1.
static inline int32_t
__syscall(uint8_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
          uint32_t a5, uint32_t a6)
{
  register int32_t r0 asm("r0") = a1;
  register int32_t r1 asm("r1") = a2;
  register int32_t r2 asm("r2") = a3;
  register int32_t r3 asm("r3") = a4;
  register int32_t r4 asm("r4") = a5;
  register int32_t r5 asm("r5") = a6;

  asm volatile("svc %1\n"
               : "=r"(r0)
               : "I" (num),
                 "r" (r0),
                 "r" (r1),
                 "r" (r2),
                 "r" (r3),
                 "r" (r4),
                 "r" (r5)
               : "memory", "cc");

  if (r0 < 0) {
    errno = -r0;
    return -1;
  }

  return r0;
}

#endif  // !_SYS_SYSCALL_H
