#
# Makefile fragment for the kernel
#

KERNEL_CFLAGS  := $(CFLAGS) $(INIT_CFLAGS) -Ikernel/include -D__AG_KERNEL__
KERNEL_LDFLAGS := $(LDFLAGS) -T kernel/kernel.ld -nostdlib

ifdef PROCESS_NAME
	KERNEL_MAIN_CFLAGS := -DPROCESS_NAME=$(PROCESS_NAME)
endif

KERNEL_SRCFILES :=	\
	kernel/drivers/console/kbd.c \
	kernel/drivers/console/display.c \
	kernel/drivers/console/pl011.c \
	kernel/drivers/console/pl050.c \
	kernel/drivers/console/pl111.c \
	kernel/drivers/console/console.c \
	kernel/drivers/console/serial.c \
	kernel/drivers/rtc/ds1338.c \
	kernel/drivers/rtc/sbcon.c \
	kernel/drivers/rtc/rtc.c \
	kernel/drivers/sd/pl180.c \
	kernel/drivers/sd/sd.c \
	kernel/drivers/eth.c \
	kernel/gic.c \
	kernel/fs/ext2_bitmap.c \
	kernel/fs/ext2_block_alloc.c \
	kernel/fs/ext2_inode_alloc.c \
	kernel/fs/ext2_inode.c \
	kernel/fs/ext2.c \
	kernel/fs/buf.c \
	kernel/fs/file.c \
	kernel/fs/inode.c \
	kernel/fs/path.c \
	kernel/mm/mmu.c \
	kernel/mm/page.c \
	kernel/mm/kmem.c \
	kernel/mm/vm.c \
	kernel/core/cpu.c \
	kernel/core/kmutex.c \
	kernel/core/task.c \
	kernel/core/spinlock.c \
	kernel/core/wchan.c \
	kernel/context.S \
	kernel/cprintf.c \
	kernel/entry.S \
	kernel/entry_trtab.c \
	kernel/exec.c \
	kernel/irq.c \
	kernel/kdebug.c \
	kernel/monitor.c \
	kernel/process.c \
	kernel/ptimer.c \
	kernel/syscall.c \
	kernel/trapentry.S \
	kernel/trap.c \
	kernel/main.c

KERNEL_SRCFILES += \
	lib/stdio/__printf.c \
	lib/string/memcmp.c \
	lib/string/memcpy.c \
	lib/string/memmove.c \
	lib/string/memset.c \
	lib/string/strchr.c \
	lib/string/strcmp.c \
	lib/string/strcpy.c \
	lib/string/strlen.c \
	lib/string/strncmp.c \
	lib/string/strncpy.c \
	lib/string/strnlen.c \
	lib/string/strpbrk.c \
	lib/string/strspn.c \
	lib/string/strtok.c \
	lib/time/gmtime.c \
	lib/time/mktime.c

KERNEL_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(KERNEL_SRCFILES))
KERNEL_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(KERNEL_OBJFILES))
KERNEL_OBJFILES := $(patsubst $(OBJ)/lib/%, $(OBJ)/kernel/%, $(KERNEL_OBJFILES))

# Embed the initial process directly into the kernel binary
ifdef PROCESS_NAME
	KERNEL_BINFILES := user/$(PROCESS_NAME)
else
	KERNEL_BINFILES := user/init
endif
KERNEL_BINFILES := $(patsubst %, $(OBJ)/%, $(KERNEL_BINFILES))

# Embed the VGA font to print characters on LCD
KERNEL_BINFILES += kernel/drivers/console/vga_font.psf

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

# Rebuild main.c when the initial process name changes
$(OBJ)/kernel/main.o: override KERNEL_CFLAGS += $(KERNEL_MAIN_CFLAGS)
$(OBJ)/kernel/main.o: $(OBJ)/.vars.KERNEL_MAIN_CFLAGS

$(OBJ)/kernel/kernel: $(KERNEL_OBJFILES) $(KERNEL_BINFILES) kernel/kernel.ld
	@echo "+ LD [KERNEL] $@"
	$(V)$(LD) -o $@ $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(LIBGCC) \
		-b binary $(KERNEL_BINFILES)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
