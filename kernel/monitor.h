#ifndef KERNEL_MONITOR_H
#define KERNEL_MONITOR_H

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

#endif  // !KERNEL_MONITOR_H
