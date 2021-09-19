#include "syscall.h"

#define SYSCALL(name)     \
  .globl name;            \
name:                     \
  svc   #SYS_ ## name;    \
  bx    lr

SYSCALL(cputs)