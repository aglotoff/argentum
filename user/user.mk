USER_CFLAGS  := $(CFLAGS) -Wno-return-local-addr
USER_LDFLAGS := $(LDFLAGS) -T arch/$(ARCH)/user/user.ld -nostdlib -L$(OBJ)/lib

USER_SRCFILES := user/init.c

USER_APPS := $(patsubst %.c, $(OBJ)/%, $(USER_SRCFILES))

$(OBJ)/user/%.o: user/%.c $(OBJ)/.vars.USER_CFLAGS
	@echo "+ CC [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%.o: user/%.S $(OBJ)/.vars.USER_CFLAGS
	@echo "+ AS [USER] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%: $(OBJ)/user/%.o $(OBJ)/arch/$(ARCH)/lib/crt0.o \
		$(OBJ)/lib/libc.a $(OBJ)/.vars.USER_LDFLAGS
	@echo "+ LD [USER] $@"
	@mkdir -p $(@D)
	$(V)$(LD) -o $@ $(USER_LDFLAGS) \
		$(OBJ)/arch/$(ARCH)/lib/crt0.o $@.o \
		-L$(dir $(LIBGCC)) -lc -lgcc
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
