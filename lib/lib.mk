#
# Makefile fragment for the library
#

include arch/$(ARCH)/lib/lib.mk

LIB_CFLAGS := $(CFLAGS)

LIB_SRCFILES := \
	lib/_exit.c \
	lib/cwrite.c \
	lib/memcmp.c \
	lib/memmove.c \
	lib/memset.c \
	lib/snprintf.c \
	lib/strchr.c \
	lib/strlen.c \
	lib/strncpy.c \
	lib/vsnprintf.c \
	lib/xprintf.c

LIB_SRCFILES += $(ARCH_LIB_SRCFILES)

LIB_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(LIB_SRCFILES))
LIB_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(LIB_OBJFILES))

CRT_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(CRT_SRCFILES))

$(OBJ)/lib/%.o: lib/%.c $(OBJ)/.vars.LIB_CFLAGS
	@echo "+ CC [LIB] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJ)/arch/$(ARCH)/lib/%.o: arch/$(ARCH)/lib/%.c $(OBJ)/.vars.LIB_CFLAGS
	@echo "+ CC [LIB] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJ)/arch/$(ARCH)/lib/%.o: arch/$(ARCH)/lib/%.S $(OBJ)/.vars.LIB_CFLAGS
	@echo "+ AS [LIB] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJ)/lib/libc.a: $(LIB_OBJFILES)
	@echo "+ AR [LIB] $@"
	$(V)$(AR) r $@ $(LIB_OBJFILES)
