#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

struct Trapframe *tf;

int syscall(struct Trapframe *tf);

#endif  // !KERNEL_SYSCALL_H
