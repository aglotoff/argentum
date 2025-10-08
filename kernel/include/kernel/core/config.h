/**
 * @file config.h
 * @brief Core kernel configuration parameters and system hooks.
 *
 * This header defines global configuration constants for system-level
 * customization.
 */

#ifndef __INCLUDE_KERNEL_CORE_CONFIG_H__
#define __INCLUDE_KERNEL_CORE_CONFIG_H__

#include <stddef.h>         // NULL, size_t, offsetof
#include <stdint.h>         // integer types

#include <errno.h>          // error codes
#include <limits.h>         // NZERO
#include <string.h>         // memmove

#include <kernel/console.h> // _panic, _warn

/**
 * @brief Maximum number of logical CPUs supported by the kernel.
 *
 * This value defines the upper bound for multi-core scheduling and
 * per-CPU structures.
 */
#define K_CPU_MAX   4

/**
 * @brief Maximum number of distinct task priority levels.
 */
#define K_TASK_MAX_PRIORITIES  (2 * NZERO)

/* -------------------------------------------------------------------------- */
/*                      Kernel-level error code aliases                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Operation would block or resource temporarily unavailable.
 *
 * Indicates that the requested operation could not complete immediately,
 * but may succeed if retried later. Commonly returned by non-blocking
 * synchronization primitives or message queues.
 */
#define K_ERR_AGAIN     (-EAGAIN)

/**
 * @brief Resource deadlock condition detected.
 *
 * Returned when an operation would result in a deadlock — for example,
 * when attempting to lock a mutex that would block the current task
 * while it already holds a conflicting lock.
 */
#define K_ERR_DEADLK    (-EDEADLK)

/**
 * @brief Invalid argument passed to a kernel API.
 *
 * Indicates that one or more parameters were invalid, out of range,
 * or inconsistent with the current system state.
 */
#define K_ERR_INVAL     (-EINVAL)

/**
 * @brief Operation timed out.
 *
 * Returned when a blocking call exceeds its specified timeout period
 * before the condition it was waiting on became true.
 */
#define K_ERR_TIMEDOUT  (-ETIMEDOUT)

/* -------------------------------------------------------------------------- */
/*                                 Utilities                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Wrapper around standard memory move function.
 */
#define k_memmove     memmove

/**
 * @brief Kernel panic macro.
 *
 * Prints a fatal error message and halts the system.
 */
#define k_panic(...)  _panic(__FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Kernel warning macro.
 *
 * Prints a non-fatal diagnostic message.
 */
#define k_warn(...)   _warn(__FILE__, __LINE__, __VA_ARGS__)

/* -------------------------------------------------------------------------- */
/*                               Task Hooks                                   */
/* -------------------------------------------------------------------------- */

struct KTask;

void on_task_destroy(struct KTask *);
void on_sched_before_switch(struct KTask *);
void on_sched_after_switch(struct KTask *);
void on_sched_idle(void);

/**
 * @brief Called when a task is destroyed.
 */
#define K_ON_TASK_DESTROY         on_task_destroy

/**
 * @brief Called immediately before performing a context switch.
 *
 * This hook is invoked in the **scheduler context** just before the low-level
 * architecture switch occurs. It is typically used for tracing, profiling, or
 * performing bookkeeping actions prior to leaving the task's context.
 *
 * @param task Pointer to the task that will be scheduled next.
 */
#define K_ON_SCHED_BEFORE_SWITCH   on_sched_before_switch

/**
 * @brief Called immediately after returning from a context switch.
 *
 * This hook is called in the **scheduler context** once control returns
 * following a context switch. It runs *after* the target task has executed and
 * yielded or been preempted.
 *
 * Typical uses include collecting runtime statistics or performing deferred
 * cleanup that should occur after another task has run.
 *
 * @param task Pointer to the task that was previously running.
 */
#define K_ON_SCHED_AFTER_SWITCH    on_sched_after_switch

/**
 * @brief Called when the scheduler enters the idle state.
 */
#define K_ON_SCHED_IDLE            on_sched_idle

#endif  // !__INCLUDE_KERNEL_CORE_CONFIG_H__
