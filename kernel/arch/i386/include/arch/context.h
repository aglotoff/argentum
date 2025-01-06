#ifndef __ARCH_I386_CONTEXT_H__
#define __ARCH_I386_CONTEXT_H__

#include <stdint.h>

/**
 * Saved registers for kernel context switches (SP is saved implicitly).
 * See https://wiki.osdev.org/Calling_Conventions
 */
struct Context {
  // Below is floating-point context aligned on 16-byte boundary
  uint32_t edi;
  uint32_t esi;
  uint32_t ebx;
  uint32_t ebp;
  uint32_t eip;
};

#endif  // !__ARCH_I386_CONTEXT_H__
