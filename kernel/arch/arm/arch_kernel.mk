KERNEL_SRCFILES += \
	kernel/arch/${ARCH}/core/arch_cpu.c \
	kernel/arch/${ARCH}/core/arch_irq.c \
	kernel/arch/${ARCH}/core/arch_spinlock.c \
	kernel/arch/${ARCH}/core/arch_switch.S \
	kernel/arch/${ARCH}/drivers/pl011.c \
	kernel/arch/${ARCH}/drivers/pl050.c \
	kernel/arch/${ARCH}/drivers/pl111.c \
	kernel/arch/${ARCH}/drivers/ds1338.c \
	kernel/arch/${ARCH}/drivers/sbcon.c \
	kernel/arch/${ARCH}/drivers/pl180.c \
	kernel/arch/${ARCH}/drivers/lan9118.c \
	kernel/arch/${ARCH}/drivers/gic.c \
	kernel/arch/${ARCH}/drivers/ptimer.c \
	kernel/arch/${ARCH}/drivers/sp804.c \
	kernel/arch/${ARCH}/mach/realview/realview.c \
	kernel/arch/${ARCH}/mach/mach.c \
	kernel/arch/${ARCH}/mm/arch_vm.c \
	kernel/arch/${ARCH}/process/arch_signal.c \
	kernel/arch/${ARCH}/arch_interrupt.c \
	kernel/arch/${ARCH}/arch_monitor.c \
	kernel/arch/${ARCH}/arch_time.c \
	kernel/arch/${ARCH}/arch_trap.c \
	kernel/arch/${ARCH}/entry.S \
	kernel/arch/${ARCH}/trapentry.S

KERNEL_CFLAGS += -mapcs-frame -Ikernel/arch/${ARCH}/include
KERNEL_LDFILE := kernel/arch/${ARCH}/kernel.ld
