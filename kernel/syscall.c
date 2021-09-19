#include <syscall.h>

#include "console.h"
#include "syscall.h"
#include "trap.h"

int
fetch_int(uintptr_t addr, int *ip)
{
  *ip = *(int *) addr;
  return 0;
}

int
sys_cputs(struct Trapframe *tf)
{
  cprintf("%s", tf->r0);
  return 0;
}

static int (*syscalls[])(struct Trapframe *tf) = {
  [SYS_cputs] = sys_cputs,
};

#define NSYSCALLS ((int) ((sizeof syscalls) / (sizeof syscalls[0])))

int
fetch_syscall_num(uintptr_t pc)
{
  int num;

  if (fetch_int(pc - 4, &num) < 0)
    return -1;
  return num & 0xFFFFFF;
}

int
syscall(struct Trapframe *tf)
{
  int num;

  if ((num = fetch_syscall_num(tf->pc)) < 0)
    return num;

  if (num < NSYSCALLS && syscalls[num])
    return syscalls[num](tf);
  
  cprintf("Unknown system call %d\n");
  return -1;
}
