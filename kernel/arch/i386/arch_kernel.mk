KERNEL_SRCFILES += \
	kernel/arch/${ARCH}/core/arch_cpu.c \
	kernel/arch/${ARCH}/core/arch_irq.c \
	kernel/arch/${ARCH}/core/arch_spinlock.c \
	kernel/arch/${ARCH}/core/arch_switch.c \
	kernel/arch/${ARCH}/core/arch_thread.c \
	kernel/arch/${ARCH}/drivers/i8042.c \
	kernel/arch/${ARCH}/drivers/i8253.c \
	kernel/arch/${ARCH}/drivers/i8259.c \
	kernel/arch/${ARCH}/drivers/ide.c \
	kernel/arch/${ARCH}/drivers/vga.c \
	kernel/arch/${ARCH}/mm/arch_vm.c \
	kernel/arch/$(ARCH)/process/arch_process.c \
	kernel/arch/$(ARCH)/process/arch_signal.c \
	kernel/arch/${ARCH}/arch_init.c \
	kernel/arch/${ARCH}/arch_interrupt.c \
	kernel/arch/${ARCH}/arch_console.c \
	kernel/arch/${ARCH}/arch_monitor.c \
	kernel/arch/${ARCH}/arch_time.c \
	kernel/arch/${ARCH}/arch_trap.c \
	kernel/arch/${ARCH}/arch_syscall.c \
	kernel/arch/${ARCH}/arch_tty.c \
	kernel/arch/${ARCH}/entry.S \
	kernel/arch/${ARCH}/trap_entry.S

KERNEL_CFLAGS += -Ikernel/arch/${ARCH}/include
KERNEL_LDFILE := kernel/arch/${ARCH}/kernel.ld
