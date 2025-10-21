#ifndef __INCLUDE_KERNEL_CORE_TYPES_H__
#define __INCLUDE_KERNEL_CORE_TYPES_H__

#include <kernel/core/list.h>

/**
 * @brief Kernel tick type.
 *
 * Represents the system tick counter or duration in kernel time units.
 */
typedef unsigned long long k_tick_t;

enum {
  /**
   * @brief Wakeable sleep mode.
   *
   * The task may be woken up before its wait condition or timeout is satisfied
   * by an explicit call to `k_task_wake()`.
   *
   * This mode is typically used for operations that can be canceled,
   * interrupted, or require responsiveness to asynchronous wake events.
   */
  K_SLEEP_WAKEABLE = (0 << 0),

  /**
   * @brief Unwakeable sleep mode.
   *
   * The task remains blocked until its wait condition or timeout is met,
   * regardless of any external wake attempts.
   *
   * This mode is used when a task must remain asleep until the awaited
   * resource becomes available, ensuring deterministic blocking behavior.
   */
  K_SLEEP_UNWAKEABLE = (1 << 0),
};

struct KTimeoutEntry {
  struct KListLink link;
  k_tick_t remain;
};

/**
 * @brief Obtain a pointer to the containing structure from a pointer.
 *
 * Converts a field pointer back to the parent structure that contains it.
 *
 * @param p Pointer to the structure member.
 * @param type Type of the parent structure.
 * @param member Name of the structure field within the structure.
 *
 * @return Pointer to the parent structure.
 */
#define K_CONTAINER_OF(p, type, member) \
  ((type *) ((k_size_t) (p) - k_offsetof(type, member)))

#endif  // !__INCLUDE_KERNEL_CORE_TYPES_H__
