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
ARCH    := arm

include arch/$(ARCH)/config.mk

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
CFLAGS += $(ARCH_CFLAGS)

# libgcc contains auxiliary helper routines and runtime support
LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# Common linker flags
LDFLAGS := $(ARCH_LDFLAGS)

# Kernel executable name
KERNEL := $(OBJ)/kernel/argentum

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

include arch/$(ARCH)/qemu.mk

clean:
	rm -rf $(OBJ)

.PHONY: all qemu qemu-gdb clean
