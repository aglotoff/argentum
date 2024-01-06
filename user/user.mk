USER_CFLAGS  := $(CFLAGS) -Wno-return-local-addr -Wno-char-subscripts -D__OSDEV__

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

USER_APPS := $(patsubst user/%.c, $(SYSROOT)/%, $(USER_SRCFILES))

$(OBJ)/user/%.o: user/%.c $(OBJ)/.vars.USER_CFLAGS
	@echo "+ CC [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%.o: user/%.S $(OBJ)/.vars.USER_CFLAGS
	@echo "+ AS [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%: $(OBJ)/user/%.o $(OBJ)/.vars.USER_LDFLAGS $(SYSROOT)/usr/lib/libc.a
	@echo "+ LD [USER] $@"
	@mkdir -p $(@D)
	$(V)$(CC) $(CFLAGS) -o $@ $<
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(SYSROOT)/%: $(OBJ)/user/%
	@mkdir -p $(@D)
	cp $< $@

all-user: $(USER_APPS)

clean-user:
	rm -rf $(OBJ)/user
