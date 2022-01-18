USER_CFLAGS  := $(CFLAGS) -Wno-return-local-addr
USER_LDFLAGS := $(LDFLAGS) -T user/user.ld -nostdlib

USER_SRCFILES := \
	user/hello.c \
	user/init.c

USER_SRCFILES += \
	user/bin/sh.c

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
	user/test/setjmp.c \
	user/test/stdlib.c \
	user/test/string.c

USER_APPS := $(patsubst %.c, $(OBJ)/%, $(USER_SRCFILES))

$(OBJ)/user/%.o: user/%.c $(OBJ)/.vars.USER_CFLAGS
	@echo "+ CC  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%.o: user/%.S $(OBJ)/.vars.USER_CFLAGS
	@echo "+ AS  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%: $(OBJ)/user/%.o $(OBJ)/lib/entry.o $(OBJ)/lib/libc.a \
		$(OBJ)/.vars.USER_LDFLAGS
	@echo "+ LD  $@"
	@mkdir -p $(@D)
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(OBJ)/lib/entry.o $@.o -L$(OBJ)/lib -lc \
		$(LIBGCC)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
