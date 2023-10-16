#
# Makefile fragment for the kernel
#

KERNEL_CFLAGS  := $(CFLAGS) -D__AG_KERNEL__
KERNEL_CFLAGS  += -I kernel/include -I kernel/arch/arm/include

KERNEL_LDFLAGS := $(LDFLAGS) -T kernel/kernel.ld -nostdlib

KERNEL_SRCFILES :=	\
	kernel/arch/arm/arch_console.c \
	kernel/arch/arm/arch_irq.c \
	kernel/arch/arm/arch_spinlock.c \
	kernel/arch/arm/arch_trap.c \
	kernel/arch/arm/arch_vm.c \
	kernel/arch/arm/entry_pgdir.c \
	kernel/arch/arm/entry.S \
	kernel/arch/arm/gic.c \
	kernel/arch/arm/mptimer.c \
	kernel/arch/arm/pl011.c \
	kernel/arch/arm/arch_smp.c \
	kernel/arch/arm/trapentry.S \
	kernel/irq.c \
	kernel/kprintf.c \
	kernel/main.c \
	kernel/page.c \
	kernel/smp.c \
	kernel/spinlock.c \
	kernel/vm.c

KERNEL_SRCFILES += \
	lib/memmove.c \
	lib/memset.c \
	lib/snprintf.c \
	lib/strchr.c \
	lib/strlen.c \
	lib/vsnprintf.c \
	lib/xprintf.c

KERNEL_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(KERNEL_SRCFILES))
KERNEL_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(KERNEL_OBJFILES))
KERNEL_OBJFILES := $(patsubst $(OBJ)/lib/%, $(OBJ)/kernel/%, $(KERNEL_OBJFILES))

$(OBJ)/kernel/%.o: kernel/%.c $(OBJ)/.vars.KERNEL_CFLAGS
	@echo "+ CC [KERNEL] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/kernel/%.o: kernel/%.S $(OBJ)/.vars.KERNEL_CFLAGS
	@echo "+ AS [KERNEL] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/kernel/%.o: lib/%.c $(OBJ)/.vars.KERNEL_CFLAGS
	@echo "+ CC [KERNEL] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(KERNEL): $(KERNEL_OBJFILES) $(KERNEL_BINFILES) kernel/kernel.ld
	@echo "+ LD [KERNEL] $@"
	$(V)$(LD) -o $@.elf $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(LIBGCC) \
		-b binary $(KERNEL_BINFILES)
	$(V)$(OBJCOPY) -O binary $@.elf $@
	$(V)$(OBJDUMP) -S $@.elf > $@.asm
	$(V)$(NM) -n $@.elf > $@.sym
