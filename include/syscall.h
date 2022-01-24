#ifndef __INCLUDE_SYSCALL_H__
#define __INCLUDE_SYSCALL_H__

#include <errno.h>
#include <stdint.h>

// System call numbers
#define __SYS_FORK        1
#define __SYS_EXEC        2
#define __SYS_WAIT        3
#define __SYS_EXIT        4
#define __SYS_GETPID      5
#define __SYS_GETPPID     6
#define __SYS_TIME        7
#define __SYS_GETDENTS    8
#define __SYS_CHDIR       9
#define __SYS_OPEN        10
#define __SYS_STAT        11
#define __SYS_CLOSE       12
#define __SYS_READ        13
#define __SYS_WRITE       14
#define __SYS_SBRK        15

// Generic system call: pass system call number as an immediate operand of the
// SVC instruction, and up to three parameters in R0, R1, R2.
// Interrupt kernel with the SVC instruction.
//
// If a negative value is returned, set the errno variable and return -1.
static inline int32_t
__syscall(uint8_t num, uint32_t a1, uint32_t a2, uint32_t a3)
{
  register int32_t r0 asm("r0") = a1;
  register int32_t r1 asm("r1") = a2;
  register int32_t r2 asm("r2") = a3;

  asm volatile("svc %1\n"
               : "=r"(r0)
               : "I" (num),
                 "r" (r0),
                 "r" (r1),
                 "r" (r2)
               : "memory", "cc");

  if (r0 < 0) {
    errno = -r0;
    return -1;
  }

  return r0;
}

#endif  // !__INCLUDE_SYSCALL_H__
