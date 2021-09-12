#
# Makefile fragment for the kernel
#

KERNEL_CFLAGS := $(CFLAGS) -DKERNEL

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
	kernel/kdebug.c \
	kernel/monitor.c \
	kernel/main.c \
	lib/stdio/xprintf.c \
	lib/string/memmove.c \
	lib/string/memset.c \
	lib/string/strchr.c \
	lib/string/strcmp.c \
	lib/string/strlen.c

KERNEL_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(KERNEL_SRCFILES))
KERNEL_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(KERNEL_OBJFILES))
KERNEL_OBJFILES := $(patsubst $(OBJ)/lib/%, $(OBJ)/kernel/%, $(KERNEL_OBJFILES))

# Embed the VGA font to print characters on LCD
KERNEL_BINFILES := kernel/vga_font.psf

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
	$(V)$(LD) -o $@ $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(LIBGCC) \
		-b binary $(KERNEL_BINFILES)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
