USER_CFLAGS  := $(CFLAGS) -Wno-return-local-addr
USER_LDFLAGS := $(LDFLAGS) -T user/user.ld -nostdlib -L$(OBJ)/lib

USER_SRCFILES :=

# USER_SRCFILES += \
# 	user/hello.c

USER_SRCFILES += \
	user/bin/cat.c \
	user/bin/chmod.c \
	user/bin/date.c \
	user/bin/echo.c \
	user/bin/env.c \
	user/bin/link.c \
	user/bin/ls.c \
	user/bin/mkdir.c \
	user/bin/pwd.c \
	user/bin/rm.c \
	user/bin/rmdir.c \
	user/bin/sh.c \
	user/bin/uname.c

USER_SRCFILES += \
	user/test/faultread.c \
	user/test/faultreadkernel.c \
	user/test/faultwrite.c \
	user/test/faultwritekernel.c \
	user/test/ctype.c \
	user/test/errno.c \
	user/test/float.c \
	user/test/fork.c \
	user/test/limits.c \
	user/test/math.c \
	user/test/setjmp.c \
	user/test/sort.c \
	user/test/stdlib.c \
	user/test/string.c

USER_APPS := $(patsubst %.c, $(OBJ)/%, $(USER_SRCFILES))

$(OBJ)/user/%.o: user/%.c $(OBJ)/.vars.USER_CFLAGS
	@echo "+ CC [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%.o: user/%.S $(OBJ)/.vars.USER_CFLAGS
	@echo "+ AS [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%: $(OBJ)/user/%.o $(OBJ)/lib/crt0.o $(OBJ)/lib/crti.o \
		$(OBJ)/lib/crtn.o $(OBJ)/lib/libc.a $(SYSROOT) $(OBJ)/.vars.USER_LDFLAGS
	@echo "+ LD [USER] $@"
	@mkdir -p $(@D)
	$(V)$(LD) -o $@ $(USER_LDFLAGS) \
		$(OBJ)/lib/crt0.o $(OBJ)/lib/crti.o $@.o $(OBJ)/lib/crtn.o \
		-L$(dir $(LIBGCC)) -lc -lgcc
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
