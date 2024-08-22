#ifndef __KERNEL_INCLUDE_KERNEL_TRAP_H__
#define __KERNEL_INCLUDE_KERNEL_TRAP_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

// Trap numbers
#define T_RESET     0         ///< Reset
#define T_UNDEF     1         ///< Undefined Instruction
#define T_SWI       2         ///< Supervisor Call (SVC)
#define T_PABT      3         ///< Prefetch Abort
#define T_DABT      4         ///< Data Abort
#define T_UNUSED    5         ///< Not Used
#define T_IRQ       6         ///< IRQ (Interrupt)
#define T_FIQ       7         ///< FIQ (Fast Interrupt)

// IRQ numbers
// #define IRQ_PTIMER    29
#define IRQ_UART0     44
#define IRQ_MCIA      49
#define IRQ_MCIB      50
#define IRQ_KMI0      52
#define IRQ_ETH       60
#define IRQ_MAX       64

#ifndef __ASSEMBLER__

#include <stdint.h>

#include <kernel/semaphore.h>

struct KThread;

/** Generic Trap Frame */
struct TrapFrame {
  uint32_t  psr;              ///< Saved PSR
  uint32_t  trapno;           ///< Trap number
  uint32_t  sp;               ///< Saved SP
  uint32_t  lr;               ///< Saved LR
  uint32_t  r0;               ///< Saved R0
  uint32_t  r1;               ///< Saved R1
  uint32_t  r2;               ///< Saved R2
  uint32_t  r3;               ///< Saved R3
  uint32_t  r4;               ///< Saved R4
  uint32_t  r5;               ///< Saved R5
  uint32_t  r6;               ///< Saved R6
  uint32_t  r7;               ///< Saved R7
  uint32_t  r8;               ///< Saved R8
  uint32_t  r9;               ///< Saved R9
  uint32_t  r10;              ///< Saved R10
  uint32_t  r11;              ///< Saved R11
  uint32_t  r12;              ///< Saved R12
  uint32_t  pc;               ///< Saved PC
};

void trap(struct TrapFrame *tf);

/**
 * Print the contents of the provided trap frame.
 */
void print_trapframe(struct TrapFrame *tf);

void interrupt_init(void);
void interrupt_init_percpu(void);
void interrupt_dispatch(void);
void interrupt_ipi(void);
void interrupt_mask(int);
void interrupt_unmask(int);
void interrupt_enable(int, int);

struct ISRThread {
  struct KSemaphore semaphore;
  void (*handler)(void *);
  void *handler_arg;
};

void interrupt_attach_thread(struct ISRThread *, int, void (*)(void *), void *);

int timer_irq(void *);
int ipi_irq(void *);

#endif  // !__ASSEMBLER__

#endif  // __KERNEL_INCLUDE_KERNEL_TRAP_H__
