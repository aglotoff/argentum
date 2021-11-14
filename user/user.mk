USER_CFLAGS  := $(CFLAGS) -Wno-return-local-addr
USER_LDFLAGS := $(LDFLAGS) -T user/user.ld -nostdlib

$(OBJ)/user/%.o: user/%.c
	@echo "+ CC  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%.o: user/%.S
	@echo "+ AS  $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(OBJ)/user/%: $(OBJ)/user/%.o $(OBJ)/lib/entry.o $(OBJ)/lib/libc.a user/user.ld
	@echo "+ LD  $@"
	@mkdir -p $(@D)
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(OBJ)/lib/entry.o $@.o -L$(OBJ)/lib -lc \
		$(LIBGCC)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
