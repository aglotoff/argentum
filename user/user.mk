USER_CFLAGS  := $(CFLAGS) -Wno-return-local-addr -Wno-char-subscripts -D__OSDEV__

USER_LDFLAGS := $(LDFLAGS)
ifneq ($(LD),arm-none-osdev-ld)
	USER_LDFLAGS += -T user/user.ld -nostdlib -L$(OBJ)/lib
endif

USER_SRCFILES :=

USER_SRCFILES += \
 	user/hello.c

USER_SRCFILES += \
	user/bin/sh.c \
	user/bin/cat.c \
	user/bin/date.c \
        user/bin/echo.c \
	user/bin/ls.c \
	user/bin/chmod.c \
	user/bin/mkdir.c \
	user/bin/uname.c \
	user/bin/rmdir.c \
	user/bin/link.c \
	user/bin/pwd.c \
	user/bin/rm.c

#USER_SRCFILES += \
#	user/test/faultread.c \
#	user/test/faultreadkernel.c \
#	user/test/faultwrite.c \
#	user/test/faultwritekernel.c \
#	user/test/ctype.c \
#	user/test/errno.c \
#	user/test/float.c \
#	user/test/fnmatch.c \
#	user/test/fork.c \
#	user/test/limits.c \
#	user/test/math.c \
#	user/test/seek.c \
#	user/test/setjmp.c \
#	user/test/sort.c \
#	user/test/stdlib.c \
#	user/test/string.c

USER_APPS := $(patsubst %.c, $(OBJ)/%, $(USER_SRCFILES))

$(OBJ)/user/%: user/%.c $(OBJ)/.vars.USER_CFLAGS
	@echo "+ CC [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -o $@ $<

$(OBJ)/user/%: user/%.S $(OBJ)/.vars.USER_CFLAGS
	@echo "+ AS [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -o $@ $<

