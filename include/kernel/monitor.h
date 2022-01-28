#ifndef KERNEL_MONITOR_H
#define KERNEL_MONITOR_H

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/monitor.h
 * 
 * Simple interactive kernel monitor.
 */

struct Trapframe;

/**
 * Enter the kernel monitor.
 */
void monitor(struct Trapframe *);

/**
 * Display the list of commands supprted by the kernel monitor.
 */
int mon_help(int, char **, struct Trapframe *);

/**
 * Display info about the kernel executable
 */
int mon_kerninfo(int, char **, struct Trapframe *);

/**
 * Display the stack backtrace.
 */
int mon_backtrace(int, char **, struct Trapframe *);

int mon_poolinfo(int, char **, struct Trapframe *);

#endif  // !KERNEL_MONITOR_H
