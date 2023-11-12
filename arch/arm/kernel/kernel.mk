ARCH_KERNEL_SRCFILES :=	\
	arch/$(ARCH)/kernel/console.c \
	arch/$(ARCH)/kernel/entry_pgdir.c \
	arch/$(ARCH)/kernel/entry.S \
	arch/$(ARCH)/kernel/gic.c \
	arch/$(ARCH)/kernel/irq.c \
	arch/$(ARCH)/kernel/mptimer.c \
	arch/$(ARCH)/kernel/pl011.c \
	arch/$(ARCH)/kernel/smp.c \
	arch/$(ARCH)/kernel/spinlock.c \
	arch/$(ARCH)/kernel/thread.c \
	arch/$(ARCH)/kernel/thread_switch.S \
	arch/$(ARCH)/kernel/trap.c \
	arch/$(ARCH)/kernel/trapentry.S \
	arch/$(ARCH)/kernel/vm.c
