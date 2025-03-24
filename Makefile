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

ifndef ARCH
	#ARCH := arm
	ARCH := i386
endif

# Common compiler flags
BASE_FLAGS := -Wall -Wextra -Werror
BASE_FLAGS += -Wno-address-of-packed-member -Wno-maybe-uninitialized

CFLAGS     := $(BASE_FLAGS) --std=gnu11
CXXFLAGS   := $(BASE_FLAGS)

include config/arch/$(ARCH)/env.mk

# Cross-compiler toolchain
TOOLPREFIX := $(HOST)-

AR      := $(TOOLPREFIX)ar
CC      := $(TOOLPREFIX)gcc
CXX     := $(TOOLPREFIX)g++
GDB     := $(TOOLPREFIX)gdb
LD      := $(TOOLPREFIX)ld
NM      := $(TOOLPREFIX)nm
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

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
	$(V)genext2fs -B 4096 -b 262144 -d $@.d -P -U $@ -N 8192

ifndef CPUS
  CPUS := 2
endif

include config/arch/$(ARCH)/qemu.mk

$(SYSROOT):
	@mkdir -p $@{,/dev,/usr{,/lib,/include}}

clean:
	rm -rf $(OBJ) $(SYSROOT)

.PRECIOUS: $(OBJ)/user/%.o

.PHONY: all all-kernel all-lib all-user \
	clean clean-fs clean-kernel clean-user clean-lib clean-ports \
	install-headers install-lib \
	fs
