#ifndef _SYS_SYSCALL_H
 #define _SYS_SYSCALL_H

// System call numbers

#define __SYS_TEST          0
#define __SYS_FORK          1
#define __SYS_EXEC          2
#define __SYS_WAIT          3
#define __SYS_EXIT          4
#define __SYS_ALARM         5
#define __SYS_GETPID        6
#define __SYS_GETPPID       7
#define __SYS_GETDENTS      8
#define __SYS_CHDIR         9
#define __SYS_FCHDIR        10
#define __SYS_OPEN          11
#define __SYS_FCNTL         12
#define __SYS_SEEK          13
#define __SYS_UMASK         14
#define __SYS_LINK          15
#define __SYS_MKNOD         16
#define __SYS_UNLINK        17
#define __SYS_RMDIR         18
#define __SYS_STAT          19
#define __SYS_CLOSE         20
#define __SYS_READ          21
#define __SYS_WRITE         22
#define __SYS_SBRK          23
#define __SYS_UNAME         24
#define __SYS_CHMOD         25
#define __SYS_FCHMOD        26
#define __SYS_CLOCK_TIME    27
#define __SYS_SOCKET        28
#define __SYS_BIND          29
#define __SYS_LISTEN        30
#define __SYS_CONNECT       31
#define __SYS_ACCEPT        32
#define __SYS_SIGPROCMASK   33
#define __SYS_KILL          34
#define __SYS_SIGACTION     35
#define __SYS_SIGRETURN     36
#define __SYS_SIGPENDING    37
#define __SYS_NANOSLEEP     38
#define __SYS_SENDTO        39
#define __SYS_RECVFROM      40
#define __SYS_SETSOCKOPT    41
#define __SYS_GETUID        42
#define __SYS_GETEUID       43
#define __SYS_GETGID        44
#define __SYS_GETEGID       45
#define __SYS_GETPGID       46
#define __SYS_SETUID        47
#define __SYS_SETEUID       48
#define __SYS_SETGID        49
#define __SYS_SETEGID       50
#define __SYS_SETPGID       51
#define __SYS_ACCESS        52
#define __SYS_PIPE          53
#define __SYS_IOCTL         54
#define __SYS_MMAP          55
#define __SYS_MPROTECT      56
#define __SYS_MUNMAP        57
#define __SYS_SELECT        58
#define __SYS_SIGSUSPEND    59
#define __SYS_GETHOSTBYNAME 60
#define __SYS_FSYNC         61
#define __SYS_FTRUNCATE     62
#define __SYS_FCHOWN        63
#define __SYS_READLINK      64
#define __SYS_TIMES         65
#define __SYS_MOUNT         66
#define __SYS_SETITIMER     67
#define __SYS_RENAME        68
#define __SYS_CHOWN         69
#define __SYS_UTIME         70

#ifndef __ASSEMBLER__

#include <errno.h>
#include <stdint.h>

#if defined(__arm__) || defined(__thumb__)

// Generic system call: pass system call number as an immediate operand of the
// SVC instruction, and up to three parameters in R0, R1, R2.
// Interrupt kernel with the SVC instruction.
//
// If a negative value is returned, set the errno variable and return -1.
static inline int32_t
__syscall_r(uint8_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
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

  return r0;
}

#elif defined(__i386__)

static inline int32_t
__syscall_r(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
            uint32_t a5, uint32_t a6)
{
  int32_t ret;

  // FIXME: add support for 6 arguments
  (void) a6;

  asm volatile("int %1\n"
		: "=a" (ret)
		: "i" (0x80),
		  "a" (num),
		  "d" (a1),
		  "c" (a2),
		  "b" (a3),
		  "D" (a4),
		  "S" (a5)
		: "cc", "memory");

  return ret;
}

#endif

static inline int32_t
__syscall(uint8_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
          uint32_t a5, uint32_t a6)
{
  int r = __syscall_r(num, a1, a2, a3, a4, a5, a6);

  if (r < 0) {
    errno = -r;
    return -1;
  }

  return r;
}

#define __syscall0(num)     \
  __syscall(num, 0, 0, 0, 0, 0, 0)
#define __syscall1(num, a1) \
  __syscall(num, (uint32_t) (a1), 0, 0, 0, 0, 0)
#define __syscall2(num, a1, a2) \
  __syscall(num, (uint32_t) (a1), (uint32_t) (a2), 0, 0, 0, 0)
#define __syscall3(num, a1, a2, a3) \
  __syscall(num, (uint32_t) (a1), (uint32_t) (a2), (uint32_t) (a3), 0, 0, 0)
#define __syscall4(num, a1, a2, a3, a4) \
  __syscall(num, (uint32_t) (a1), (uint32_t) (a2), (uint32_t) (a3), \
            (uint32_t) (a4), 0, 0)
#define __syscall5(num, a1, a2, a3, a4, a5) \
  __syscall(num, (uint32_t) (a1), (uint32_t) (a2), (uint32_t) (a3), \
            (uint32_t) (a4), (uint32_t) (a5), 0)
#define __syscall6(num, a1, a2, a3, a4, a5, a6) \
  __syscall(num, (uint32_t) (a1), (uint32_t) (a2), (uint32_t) (a3), \
            (uint32_t) (a4), (uint32_t) (a5), (uint32_t) (a6))

#endif

#endif  // !_SYS_SYSCALL_H
