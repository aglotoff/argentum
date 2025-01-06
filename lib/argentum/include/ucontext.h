#ifndef _UCONTEXT_H
# define _UCONTEXT_H

#include <signal.h>
#include <stdint.h>

#if defined(__arm__) || defined(__thumb__)

typedef struct mcontext_t {
  uint32_t  r0;
  uint32_t  r1;
  uint32_t  r2;
  uint32_t  r3;
  uint32_t  r4;
  uint32_t  r5;
  uint32_t  r6;
  uint32_t  r7;
  uint32_t  r8;
  uint32_t  r9;
  uint32_t  r10;
  uint32_t  r11;
  uint32_t  r12;
  uint32_t  sp;
  uint32_t  lr;
  uint32_t  pc;
  uint32_t  psr;

  uint32_t  s[32];
  uint32_t  fpscr;
} mcontext_t;

#elif defined(__i386__)

typedef struct mcontext_t {
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t _esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;

  // 
  uint32_t eax;
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t esp;
  uint32_t ss;

  uint8_t  fpu[512];
} mcontext_t;

#endif

typedef struct ucontext_t {
  struct ucontext_t *uc_link;
  sigset_t           uc_sigmask;
  // TODO: stack
  mcontext_t         uc_mcontext;
} ucontext_t;

#endif  // !_UCONTEXT_H
