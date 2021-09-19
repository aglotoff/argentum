#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "mmu.h"
#include "trap.h"

struct Context {
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;
};

struct Process {
  tte_t             *trtab;     ///< Translation table
  uint8_t           *kstack;    ///< Bottom of kernel stack for this process
  struct Trapframe  *tf;        ///< Trap frame for current system call
  struct Context    *context;   ///< Saved context
};

extern struct Process *curproc;

void context_switch(struct Context **old, struct Context *new);

void process_create(void *binary);
void process_run(void);

#endif  // KERNEL_PROCESS_H
