#
# Makefile fragment for the library
#

LIB_CFLAGS := $(CFLAGS)

LIB_SRCFILES :=	\
  lib/errno/errno.c \
	lib/stdio/printf.c \
	lib/stdio/snprintf.c \
	lib/stdio/vprintf.c \
	lib/stdio/vsnprintf.c \
	lib/stdio/xprintf.c \
	lib/string/memcmp.c \
	lib/string/memmove.c \
	lib/string/memset.c \
	lib/string/strchr.c \
	lib/string/strcmp.c \
	lib/string/strlen.c \
	lib/time/asctime.c \
	lib/time/gmtime.c \
	lib/time/mktime.c \
	lib/time/strftime.c \
	lib/time/time.c \
	lib/unistd/getpid.c \
	lib/unistd/getppid.c \
	lib/syscall.c

LIB_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(LIB_SRCFILES))
LIB_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(LIB_OBJFILES))

$(OBJ)/lib/%.o: lib/%.c
	@echo "+ CC  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJ)/lib/%.o: lib/%.S
	@echo "+ AS  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJ)/lib/libc.a: $(LIB_OBJFILES)
	@echo "+ AR  $@"
	$(V)$(AR) r $@ $(LIB_OBJFILES)
