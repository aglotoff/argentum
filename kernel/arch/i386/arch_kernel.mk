KERNEL_SRCFILES += \
	kernel/arch/${ARCH}/entry.S \
	kernel/arch/${ARCH}/init.c

KERNEL_CFLAGS += -I$(SYSROOT)/usr/include
KERNEL_LDFILE := kernel/arch/${ARCH}/kernel.ld
