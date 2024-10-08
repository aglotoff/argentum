# Run `make V=1 [<rest of arguments>]` to see the full-length commands executed
# by make instead of "overview" commands (such as `CC kernel/console.c`).
ifeq ($(V),1)
	V := 
else
	V := @
endif

SHELL := /bin/bash

TOP     := .
OBJ     := obj
SYSROOT := sysroot

# Cross-compiler toolchain
#
# The recommended target for the cross-compiler toolchain is "arm-none-eabi-".
# If you want to use the host tools (i.e. binutils, gcc, gdb, etc.), comment
# this line out.
TOOLPREFIX := arm-none-argentum-

# QEMU executable
QEMU := qemu-system-arm

AR      := $(TOOLPREFIX)ar
CC      := $(TOOLPREFIX)gcc
CXX     := $(TOOLPREFIX)g++
GDB     := $(TOOLPREFIX)gdb
LD      := $(TOOLPREFIX)ld
NM      := $(TOOLPREFIX)nm
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

# Common compiler flags
BASE_FLAGS := -Wall -Wextra -Werror
BASE_FLAGS += -Wno-address-of-packed-member -Wno-maybe-uninitialized
# BASE_FLAGS += -mcpu=cortex-a9 -mhard-float -mfpu=vfp
CFLAGS     := $(BASE_FLAGS) --std=gnu11
CXXFLAGS   := $(BASE_FLAGS)

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

all: all-lib all-kernel all-user

include lib/lib.mk
include kernel/kernel.mk
include user/user.mk
include ports/ports.mk
include tools/tools.mk

clean-fs:
	rm -rf $(OBJ)/fs.img

fs: clean-fs $(OBJ)/fs.img

$(OBJ)/fs.img: $(SYSROOT) $(USER_APPS)
	@echo "+ GEN $@"
	$(V)mkdir -p $@.d/{,dev,etc,home/{,root,guest},tmp}
	$(V)cp -afRd $(SYSROOT)/* $@.d/
	$(V)genext2fs -B 4096 -b 131072 -d $@.d -P -U $@

ifndef CPUS
  CPUS := 2
endif

#QEMUOPTS := -M realview-pb-a8 -m 256
QEMUOPTS := -M realview-pbx-a9 -m 256 -smp $(CPUS)
QEMUOPTS += -kernel $(KERNEL).bin
QEMUOPTS += -drive if=sd,format=raw,file=$(OBJ)/fs.img
QEMUOPTS += -nic user,hostfwd=tcp::8080-:80
QEMUOPTS += -serial mon:stdio

qemu: $(OBJ)/fs.img $(KERNEL)
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(OBJ)/fs.img $(KERNEL)
	$(QEMU) $(QEMUOPTS) -s -S

$(SYSROOT):
	@mkdir -p $@{,/dev,/usr{,/lib,/include}}

clean:
	rm -rf $(OBJ) $(SYSROOT)

.PRECIOUS: $(OBJ)/user/%.o

.PHONY: all all-kernel all-lib all-user \
	clean clean-fs clean-kernel clean-user clean-lib \
	install-headers install-lib \
	fs qemu qemu-gdb
