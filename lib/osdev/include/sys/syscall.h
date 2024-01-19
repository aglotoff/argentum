#ifndef _SYS_SYSCALL_H
 #define _SYS_SYSCALL_H

// System call numbers

#define __SYS_TEST        0
#define __SYS_FORK        1
#define __SYS_EXEC        2
#define __SYS_WAIT        3
#define __SYS_EXIT        4
#define __SYS_ALARM       5
#define __SYS_GETPID      6
#define __SYS_GETPPID     7
#define __SYS_GETDENTS    8
#define __SYS_CHDIR       9
#define __SYS_FCHDIR      10
#define __SYS_OPEN        11
#define __SYS_FCNTL       12
#define __SYS_SEEK        13
#define __SYS_UMASK       14
#define __SYS_LINK        15
#define __SYS_MKNOD       16
#define __SYS_UNLINK      17
#define __SYS_RMDIR       18
#define __SYS_STAT        19
#define __SYS_CLOSE       20
#define __SYS_READ        21
#define __SYS_WRITE       22
#define __SYS_SBRK        23
#define __SYS_UNAME       24
#define __SYS_CHMOD       25
#define __SYS_FCHMOD      26
#define __SYS_CLOCK_TIME  27
#define __SYS_SOCKET      28
#define __SYS_BIND        29
#define __SYS_LISTEN      30
#define __SYS_CONNECT     31
#define __SYS_ACCEPT      32
#define __SYS_SIGPROCMASK 33
#define __SYS_KILL        34
#define __SYS_SIGACTION   35
#define __SYS_SIGRETURN   36
#define __SYS_NANOSLEEP   37
#define __SYS_SENDTO      38
#define __SYS_RECVFROM    39
#define __SYS_SETSOCKOPT  40
#define __SYS_GETUID      41
#define __SYS_GETEUID     42
#define __SYS_GETGID      43
#define __SYS_GETEGID     44
#define __SYS_GETPGID     45
#define __SYS_SETUID      46
#define __SYS_SETEUID     47
#define __SYS_SETGID      48
#define __SYS_SETEGID     49
#define __SYS_SETPGID     50
#define __SYS_ACCESS      51
#define __SYS_PIPE        52

#ifndef __ASSEMBLER__

#include <errno.h>
#include <stdint.h>

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

#endif

#endif  // !_SYS_SYSCALL_H
