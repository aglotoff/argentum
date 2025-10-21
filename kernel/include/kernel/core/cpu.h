#ifndef __INCLUDE_KERNEL_CORE_CPU_H__
#define __INCLUDE_KERNEL_CORE_CPU_H__

/* -------------------------------------------------------------------------- */
/*                           Architecture Interface                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Retrieve the hardware CPU identifier (architecture-specific).
 *
 * This function is implemented by the architecture layer and returns the
 * index of the currently executing logical CPU. The returned value must be
 * in the range `[0, K_CPU_MAX - 1]`.
 *
 * @return The zero-based CPU ID of the current processor.
 *
 * @note On uniprocessor builds, this function may always return `0`.
 */
unsigned k_arch_cpu_id(void);

/* -------------------------------------------------------------------------- */
/*                                 Kernel API                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get the current CPU ID.
 *
 * Returns the identifier of the CPU currently executing this code.
 *
 * @return The zero-based logical CPU ID.
 *
 * @note This function is safe to call in any context, including interrupt
 *       handlers, as it typically reads from a CPU-local register or memory.
 */
static inline unsigned
k_cpu_id(void)
{
  return k_arch_cpu_id();
}

/**
 * @brief Identifier of the master CPU for system-level services.
 *
 * This macro defines the zero-based ID of the CPU that is responsible for
 * handling global kernel tasks such as processing timeouts, timers,
 * and other centralized services in an SMP system.
 */
#define K_CPU_ID_MASTER   0

#endif  // !__INCLUDE_KERNEL_CORE_CPU_H__
