# Run `make V=1 [<rest of arguments>]` to see the full-length commands executed
# by make instead of "overview" commands (such as `CC kernel/console.c`).
ifeq ($(V),1)
	V := 
else
	V := @
endif

OBJ := obj
TOP := .

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
CFLAGS := -ffreestanding -nostdlib -fno-builtin -nostdinc -I$(TOP)/include
CFLAGS += -Wall -Wextra -Werror  -Wno-address-of-packed-member
CFLAGS += --std=gnu11
CFLAGS += -O1 -mcpu=cortex-a9 -mapcs-frame -fno-omit-frame-pointer
CFLAGS += -gdwarf-3

# Common linker flags
LDFLAGS := -z max-page-size=0x1000

LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

KERNEL := $(OBJ)/kernel/kernel

# Update $(OBJ)/.vars.X if the variable X has changed since the last run.
# This allows us to rebuild targets that depend on the value of X by adding
# $(OBJ)/.vars.X to the dependency list.
$(OBJ)/.vars.%: .FORCE
	@mkdir -p $(@D)
	$(V)echo "$($*)" | cmp -s $@ || echo "$($*)" > $@
.PRECIOUS: $(OBJ)/.vars.%
.PHONY: .FORCE

all: $(KERNEL)

include kernel/kernel.mk
include lib/lib.mk
include user/user.mk

$(OBJ)/fs.img: $(USER_APPS)
	@echo "+ GEN $@"
	$(V)mkdir -p $@.d
	$(V)cp -R $^ $@.d
	$(V)mke2fs -F -b 1K -d $@.d -t ext2 $@ 32M
	$(V)rm -rf $@.d

ifndef CPUS
  CPUS := 2
endif

QEMUOPTS := -M realview-pbx-a9 -m 256 -smp $(CPUS)
QEMUOPTS += -kernel $(KERNEL) -sd $(OBJ)/fs.img
QEMUOPTS += -serial mon:stdio

qemu: $(KERNEL) $(OBJ)/fs.img
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(KERNEL) $(OBJ)/fs.img
	$(QEMU) $(QEMUOPTS) -s -S 

prep-%:
	$(V)$(MAKE) "PROCESS_NAME=$*" $(KERNEL)

run-%: prep-% $(OBJ)/fs.img
	$(QEMU) $(QEMUOPTS)

clean:
	rm -rf $(OBJ)

.PRECIOUS: $(OBJ)/user/%.o
.PHONY: all qemu clean prep-% run-%
