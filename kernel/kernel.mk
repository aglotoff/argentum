#
# Makefile fragment for the kernel
#

KERNEL_CFLAGS := $(CFLAGS) $(INIT_CFLAGS) -DKERNEL

KERNEL_LDFLAGS := $(LDFLAGS) -T kernel/kernel.ld -nostdlib

KERNEL_SRCFILES :=	\
	kernel/entry.S \
	kernel/kmi.c \
	kernel/lcd.c \
	kernel/uart.c \
	kernel/console.c \
	kernel/gic.c \
	kernel/trapentry.S \
	kernel/trap.c \
	kernel/page.c \
	kernel/kobject.c \
	kernel/vm.c \
	kernel/process.c \
	kernel/context.S \
	kernel/spinlock.c \
	kernel/syscall.c \
	kernel/sbcon.c \
	kernel/kdebug.c \
	kernel/monitor.c \
	kernel/main.c \
	lib/stdio/xprintf.c \
	lib/string/memcmp.c \
	lib/string/memmove.c \
	lib/string/memset.c \
	lib/string/strchr.c \
	lib/string/strcmp.c \
	lib/string/strlen.c \
	lib/time/mktime.c

KERNEL_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(KERNEL_SRCFILES))
KERNEL_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(KERNEL_OBJFILES))
KERNEL_OBJFILES := $(patsubst $(OBJ)/lib/%, $(OBJ)/kernel/%, $(KERNEL_OBJFILES))

# For now, embed user prorams directly into the kernel binary
KERNEL_BINFILES := user/fault_read \
                   user/fault_read_kernel \
									 user/fault_write \
                   user/fault_write_kernel \
									 user/hello \
									 user/test_ctype
KERNEL_BINFILES := $(patsubst %, $(OBJ)/%, $(KERNEL_BINFILES))

# Embed the VGA font to print characters on LCD
KERNEL_BINFILES += kernel/vga_font.psf

$(OBJ)/kernel/%.o: kernel/%.c
	@echo "+ CC  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/kernel/%.o: kernel/%.S
	@echo "+ AS  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/kernel/%.o: lib/%.c
	@echo "+ CC  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ)/kernel/kernel: $(KERNEL_OBJFILES) $(KERNEL_BINFILES) kernel/kernel.ld
	@echo "+ LD  $@"
	$(V)$(LD) -o $@.elf $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(LIBGCC) \
		-b binary $(KERNEL_BINFILES)
	$(V)$(OBJCOPY) -O binary $@.elf $@
	$(V)$(OBJDUMP) -S $@.elf > $@.asm
	$(V)$(NM) -n $@.elf > $@.sym
