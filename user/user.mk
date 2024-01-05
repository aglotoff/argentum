USER_CFLAGS  := $(CFLAGS) -Wno-return-local-addr -Wno-char-subscripts -D__OSDEV__
USER_LDFLAGS := $(LDFLAGS)

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

USER_APPS := $(patsubst %.c, $(OBJ)/%, $(USER_SRCFILES))

$(OBJ)/user/%: user/%.c $(OBJ)/.vars.USER_CFLAGS $(SYSROOT)/usr/lib/libc.a
	@echo "+ CC [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -o $@ $<

$(OBJ)/user/%: user/%.S $(OBJ)/.vars.USER_CFLAGS $(SYSROOT)/usr/lib/libc.a
	@echo "+ AS [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -o $@ $<

clean-user:
	rm -rf $(OBJ)/user
