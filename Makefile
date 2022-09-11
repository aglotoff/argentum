# Run `make V=1 [<rest of arguments>]` to see the full-length commands executed
# by make instead of "overview" commands (such as `CC kernel/console.c`).
ifeq ($(V),1)
	V := 
else
	V := @
endif

TOP     := .
OBJ     := obj
SYSROOT := sysroot

# Cross-compiler toolchain
#
# The recommended target for the cross-compiler toolchain is "arm-none-eabi-".
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
CFLAGS += -mhard-float -mfpu=vfp
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
	$(V)mkdir -p $@.d/{,etc,home/{,root,guest}}
	$(V)(pushd $(OBJ)/user; cp --parent $(patsubst $(OBJ)/user/%, %, $(USER_APPS)) $(PWD)/$@.d; popd)
	$(V)mke2fs -E root_owner=0:0 -F -b 1K -d $@.d -t ext2 $@ 32M 
	$(V)rm -rf $@.d

ifndef CPUS
  CPUS := 3
endif

QEMUOPTS := -M realview-pbx-a9 -m 256 -smp $(CPUS)
QEMUOPTS += -kernel $(KERNEL)
QEMUOPTS += -drive if=sd,format=raw,file=$(OBJ)/fs.img
#QEMUOPTS += -nic bridge,br=br1
QEMUOPTS += -serial mon:stdio

qemu: $(KERNEL) $(OBJ)/fs.img
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(KERNEL) $(OBJ)/fs.img
	$(QEMU) $(QEMUOPTS) -s -S

sysroot:
	@mkdir -p $(SYSROOT)
	@cp -R include $(SYSROOT)
	@cp -R $(OBJ)/lib $(SYSROOT)

clean:
	rm -rf $(OBJ) $(SYSROOT)

.PRECIOUS: $(OBJ)/user/%.o
.PHONY: all lib qemu clean sysroot

