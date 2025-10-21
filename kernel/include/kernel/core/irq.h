#ifndef __INCLUDE_KERNEL_CORE_IRQ_H__
#define __INCLUDE_KERNEL_CORE_IRQ_H__

/* -------------------------------------------------------------------------- */
/*                           Architecture Interface                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Check whether interrupts are currently enabled.
 *
 * Queries the processor’s interrupt state and returns a non-zero value
 * if maskable interrupts are enabled, or zero if they are disabled.
 *
 * @return Non-zero if interrupts are enabled; zero otherwise.
 *
 * @note The exact semantics may depend on the architecture. For instance,
 *       on some CPUs, only the global interrupt enable bit is checked,
 *       while others include privilege-level masking.
 */
int  k_arch_irq_is_enabled(void);

/**
 * @brief Enable maskable hardware interrupts.
 *
 * Sets the processor’s global interrupt-enable flag, allowing pending
 * or future interrupts to be delivered and serviced.
 *
 * This is typically implemented by clearing the interrupt disable bit
 * in a processor status register (e.g., `CPSR`, `mstatus`, or `PRIMASK`).
 */
void k_arch_irq_enable(void);

/**
 * @brief Disable maskable hardware interrupts.
 *
 * Clears the processor’s global interrupt-enable flag, preventing
 * further interrupt delivery until explicitly re-enabled.
 *
 * This operation is usually performed atomically by setting the interrupt
 * disable bit in a processor status register.
 */
void k_arch_irq_disable(void);

/**
 * @brief Save the current interrupt state and disable interrupts.
 *
 * Captures the processor’s interrupt enable flag and disables interrupts
 * atomically. The previous state is returned so that it can later be
 * restored using `k_arch_irq_state_restore()`.
 * 
 * The exact meaning and bit layout of the returned value are
 * architecture-defined — it may represent a processor status register,
 * a masked bitfield, or a CPU-specific interrupt-enable flag.
 *
 * @return The previous interrupt state.
 */
int  k_arch_irq_state_save(void);

/**
 * @brief Restore a previously saved interrupt state.
 *
 * Restores the processor’s interrupt enable flag using the opaque
 * architecture-defined state value previously returned by
 * `k_arch_irq_state_save()`. If the saved state indicates that
 * interrupts were enabled, they will be re-enabled; otherwise, they
 * remain disabled.
 *
 * @param state The architecture-defined interrupt state value previously
 *              returned by `k_arch_irq_state_save()`.
 */
void k_arch_irq_state_restore(int);

/* -------------------------------------------------------------------------- */
/*                                 Kernel API                                 */
/* -------------------------------------------------------------------------- */

void k_irq_state_save(void);
void k_irq_state_restore(void);
void k_irq_handler_begin(void);
void k_irq_handler_end(void);

/**
 * @brief Disable CPU interrupts.
 *
 * Disables maskable hardware interrupts on the current processor.
 *
 * Once interrupts are disabled, the processor will not respond to
 * external interrupt requests until they are explicitly re-enabled
 * via `k_irq_enable()`.
 *
 * @note These functions do not provide nesting semantics — use
 *       `k_irq_state_save()` and `k_irq_state_restore()` when
 *       nested interrupt masking is required.
 */
static inline void
k_irq_disable(void)
{
  k_arch_irq_disable();
}

/**
 * @brief Enable CPU interrupts.
 *
 * Re-enables maskable hardware interrupts on the current processor.
 * This restores normal interrupt handling after a previous call to
 * `k_irq_disable()`.
 */
static inline void
k_irq_enable(void)
{
  k_arch_irq_enable();
}

#endif  // !__INCLUDE_KERNEL_CORE_IRQ_H__
