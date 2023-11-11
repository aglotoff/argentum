#
# Makefile fragment for the kernel
#

include arch/$(ARCH)/kernel/kernel.mk

KERNEL_CFLAGS := $(CFLAGS) -D__AG_KERNEL__ -I kernel/include
KERNEL_CFLAGS += -I arch/$(ARCH)/include

KERNEL_LDFLAGS := $(LDFLAGS) -nostdlib
KERNEL_LDFLAGS += -T arch/$(ARCH)/kernel/kernel.ld

KERNEL_SRCFILES := \
	kernel/irq.c \
	kernel/kprintf.c \
	kernel/main.c \
	kernel/object.c \
	kernel/page.c \
	kernel/smp.c \
	kernel/spinlock.c \
	kernel/vm.c

KERNEL_SRCFILES += $(ARCH_KERNEL_SRCFILES)

KERNEL_SRCFILES += \
	lib/memmove.c \
	lib/memset.c \
	lib/snprintf.c \
	lib/strchr.c \
	lib/strlen.c \
	lib/strncpy.c \
	lib/vsnprintf.c \
	lib/xprintf.c

KERNEL_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(KERNEL_SRCFILES))
KERNEL_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(KERNEL_OBJFILES))
KERNEL_OBJFILES := $(patsubst $(OBJ)/lib/%, $(OBJ)/kernel/%, $(KERNEL_OBJFILES))

$(OBJ)/kernel/%.o: kernel/%.c $(OBJ)/.vars.KERNEL_CFLAGS
	@echo "+ CC [KERNEL] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/arch/$(ARCH)/kernel/%.o: arch/$(ARCH)/kernel/%.c $(OBJ)/.vars.KERNEL_CFLAGS
	@echo "+ CC [KERNEL] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/arch/$(ARCH)/kernel/%.o: arch/$(ARCH)/kernel/%.S $(OBJ)/.vars.KERNEL_CFLAGS
	@echo "+ AS [KERNEL] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/kernel/%.o: lib/%.c $(OBJ)/.vars.KERNEL_CFLAGS
	@echo "+ CC [KERNEL] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(KERNEL): $(KERNEL_OBJFILES) $(KERNEL_BINFILES) arch/$(ARCH)/kernel/kernel.ld
	@echo "+ LD [KERNEL] $@"
	$(V)$(LD) -o $@ $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(LIBGCC) \
		-b binary $(KERNEL_BINFILES)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
