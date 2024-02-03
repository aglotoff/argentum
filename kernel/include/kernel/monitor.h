#ifndef __KERNEL_INCLUDE_KERNEL_MONITOR_H__
#define __KERNEL_INCLUDE_KERNEL_MONITOR_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/monitor.h
 * 
 * Simple interactive kernel monitor.
 */

struct TrapFrame;

/**
 * Enter the kernel monitor.
 */
void monitor(struct TrapFrame *);

/**
 * Display the list of commands supprted by the kernel monitor.
 */
int mon_help(int, char **, struct TrapFrame *);

/**
 * Display info about the kernel executable
 */
int mon_kerninfo(int, char **, struct TrapFrame *);

/**
 * Display the stack backtrace.
 */
int mon_backtrace(int, char **, struct TrapFrame *);

int mon_kmeminfo(int, char **, struct TrapFrame *);

#endif  // !__KERNEL_INCLUDE_KERNEL_MONITOR_H__
