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

# The recommended target for cross compiling is "arm-none-eabi-". To use the
# host compiler, comment the following line out
TOOLPREFIX := arm-none-eabi-

AR      := $(TOOLPREFIX)ar
CC      := $(TOOLPREFIX)gcc
GDB     := $(TOOLPREFIX)gdb
LD      := $(TOOLPREFIX)ld
NM      := $(TOOLPREFIX)nm
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

# Common compiler flags
CFLAGS := -ffreestanding -nostdlib -fno-builtin -nostdinc -I$(TOP)/include
CFLAGS += -Wall -Wextra -Werror -Wno-address-of-packed-member
CFLAGS += --std=gnu11 -O1 -gdwarf-3
CFLAGS += -mcpu=cortex-a9 -mapcs-frame -fno-omit-frame-pointer
CFLAGS += -mhard-float -mfpu=vfp

# libgcc contains auxiliary helper routines and runtime support
LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# Common linker flags
LDFLAGS := -z max-page-size=0x1000

# Kernel executable name
KERNEL := $(OBJ)/kernel/argentum

# QEMU executable
QEMU := qemu-system-arm

ifndef CPUS
  CPUS := 2
endif

# Qemu options
QEMUOPTS := -M realview-pbx-a9 -m 256 -smp $(CPUS) -nographic
QEMUOPTS += -kernel $(KERNEL)
QEMUOPTS += -nic user,hostfwd=tcp::8080-:80
QEMUOPTS += -serial mon:stdio

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

qemu: $(KERNEL)
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(KERNEL)
	$(QEMU) $(QEMUOPTS) -s -S

clean:
	rm -rf $(OBJ)

.PHONY: all qemu clean
