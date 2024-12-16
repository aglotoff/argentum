USER_FLAGS  := -O2 -Wno-return-local-addr -Wno-char-subscripts -D__ARGENTUM__
USER_CFLAGS := $(CFLAGS) $(USER_FLAGS)
USER_CFLAGS := $(CFLAGS) $(USER_FLAGS)

USER_SRCFILES :=

USER_SRCFILES += \
  user/hello.c \
	#user/hello2.cc

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
	user/bin/ping.c \
	user/bin/pwd.c \
	user/bin/rm.c \
	user/bin/server.c \
	user/bin/client.c

USER_APPS := $(patsubst user/%.c, $(SYSROOT)/%, $(USER_SRCFILES))
USER_APPS := $(patsubst user/%.cc, $(SYSROOT)/%, $(USER_APPS))
USER_APPS := $(patsubst user/%.S, $(SYSROOT)/%, $(USER_APPS))

$(OBJ)/user/hello: user/hello.c $(OBJ)/.vars.USER_CFLAGS $(SYSROOT)/usr/lib/libc.a
	@echo "+ CC [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -o $@ $<

$(OBJ)/user/%.o: user/%.c $(OBJ)/.vars.USER_CFLAGS
	@echo "+ CC [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%.o: user/%.cc $(OBJ)/.vars.USER_CFLAGS
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
	$(V)$(CC) $(USER_CFLAGS) -o $@ $<
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(SYSROOT)/%: $(OBJ)/user/%
	@mkdir -p $(@D)
	cp $< $@

all-user: $(USER_APPS)

clean-user:
	rm -rf $(OBJ)/user
