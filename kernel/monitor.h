#ifndef KERNEL_MONITOR_H
#define KERNEL_MONITOR_H

/**
 * @file kernel/monitor.h
 * 
 * Simple interactive kernel monitor.
 */

/**
 * Enter the kernel monitor.
 */
void monitor(void);

/**
 * Display the list of commands supprted by the kernel monitor.
 */
int mon_help(int, char **);

/**
 * Display info about the kernel executable
 */
int mon_kerninfo(int, char **);

#endif  // !KERNEL_MONITOR_H
