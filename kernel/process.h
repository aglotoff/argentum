#ifndef __KERNEL_PROCESS_H__
#define __KERNEL_PROCESS_H__

#include <list.h>

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
  struct ListLink    link;
  int                pid;
  tte_t             *trtab;     ///< Translation table
  uint8_t           *kstack;    ///< Bottom of kernel stack for this process
  struct Trapframe  *tf;        ///< Trap frame for current system call
  struct Context    *context;   ///< Saved context
};

#define NCPU  4

/**
 * Per-CPU state.
 */
struct Cpu {
  struct Context *scheduler;    ///< Saved scheduler context
  struct Process *process;      ///< The currently running process   
  int             irq_lock;     ///<
  int             irq_flags;    ///<       
};

extern struct Cpu cpus[];

void context_switch(struct Context **old, struct Context *new);

void process_init(void);
void process_create(void *binary);
void process_yield(void);
void process_destroy(void);
void scheduler(void);

int cpuid(void);
struct Cpu *mycpu(void);
struct Process *myprocess(void);

extern int nprocesses;

#define PROCESS_PASTE3(x, y, z)   x ## y ## z

#define PROCESS_CREATE(name)                                            \
  do {                                                                  \
    extern uint8_t PROCESS_PASTE3(_binary_obj_user_, name, _start)[];   \
    process_create(PROCESS_PASTE3(_binary_obj_user_, name, _start));    \
  } while (0)

#endif  // __KERNEL_PROCESS_H__
