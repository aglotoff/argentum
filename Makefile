# Run `make V=1 [<rest of arguments>]` to see the full-length commands executed
# by make instead of "overview" commands (such as `CC kernel/console.c`).
ifeq ($(V),1)
	V := 
else
	V := @
endif

OBJ := obj
TOP := .
INC := $(TOP)/include

# Cross-compiler toolchain
#
# The recommended target for the cross-compilr toolchain is "i386-pc-elf".
# If you want to use the host tools (i.e. binutils, gcc, gdb, etc.), comment
# this line out.
TOOLPREFIX := arm-none-eabi-

# QEMU executable
QEMU := qemu-system-arm

AR      := $(TOOLPREFIX)ar
CC      := $(TOOLPREFIX)gcc
GDB     := $(TOOLPREFIX)gdb
LD      := $(TOOLPREFIX)ld
NM      := $(TOOLPREFIX)nm
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

# Common compiler flags
CFLAGS := -ffreestanding -nostdlib -fno-builtin -nostdinc -I$(INC)
CFLAGS += -Wall -Wextra -Werror 
CFLAGS += --std=gnu11
CFLAGS += -O1 -mcpu=cortex-a9 -mapcs-frame -fno-omit-frame-pointer
CFLAGS += -gdwarf-3

# Common linker flags
LDFLAGS := -z max-page-size=0x1000

LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

KERNEL := $(OBJ)/kernel/kernel

all: $(KERNEL)

include kernel/kernel.mk
include lib/lib.mk
include user/user.mk

QEMUOPTS := -M realview-pbx-a9 -m 256 -smp 1
QEMUOPTS += -kernel $(KERNEL)
QEMUOPTS += -serial mon:stdio

qemu: $(KERNEL)
	$(QEMU) $(QEMUOPTS)

clean:
	rm -rf $(OBJ)

.PHONY: all qemu clean
