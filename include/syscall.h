#ifndef __INCLUDE_SYSCALL_H__
#define __INCLUDE_SYSCALL_H__

#include <errno.h>
#include <stdint.h>

#define SYS_cwrite      1
#define SYS_exit        2
#define SYS_getpid      3
#define SYS_getppid     4

// Generic system call: pass system call number as an immediate operand of the
// SVC instruction, and up to three parameters in R0, R1, R2.
// Interrupt kernel with the SVC instruction.
//
// If a negative value is returned, set the errno variable and return -1.
static inline int
syscall(int8_t num, uint32_t a1, uint32_t a2, uint32_t a3)
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
