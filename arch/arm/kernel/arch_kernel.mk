KERNEL_SRCFILES += \
	arch/${ARCH}/kernel/core/arch_cpu.c \
	arch/${ARCH}/kernel/core/arch_irq.c \
	arch/${ARCH}/kernel/core/arch_spinlock.c \
	arch/${ARCH}/kernel/core/arch_switch.S \
	arch/${ARCH}/kernel/process/arch_signal.c \
	arch/${ARCH}/kernel/arch_interrupt.c \
	arch/${ARCH}/kernel/arch_time.c \
	arch/${ARCH}/kernel/entry.S \
	arch/${ARCH}/kernel/trapentry.S

KERNEL_CFLAGS += -mapcs-frame
KERNEL_LDFILE := arch/${ARCH}/kernel/kernel.ld
