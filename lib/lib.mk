LIB_CFLAGS := -mcpu=cortex-a9 -mhard-float -mfpu=vfp -nostdlib

NEWLIB := lib/newlib-4.4.0.20231231
NEWLIB_TARBALL := tarballs/newlib-4.4.0.20231231.tar.gz

LIB_SRCFILES := \
	lib/osdev/include/sys/dirent.h \
	lib/osdev/include/sys/utsname.h \
	lib/osdev/include/limits.h \
	lib/osdev/include/syscall.h \
	lib/osdev/crt0.S \
	lib/osdev/syscalls.c

LIB_SRCFILES := $(patsubst lib/%, $(NEWLIB)/newlib/libc/sys/%, $(LIB_SRCFILES))

$(NEWLIB_TARBALL):
	@mkdir -p $(@D)
	$(V)wget -O $@ ftp://sourceware.org/pub/newlib/newlib-4.4.0.20231231.tar.gz

$(NEWLIB): $(NEWLIB_TARBALL)
	$(V)tar xf $^ -C $(@D)
	$(V)cp -ar lib/osdev $@/newlib/libc/sys/
	$(V)pushd $@ && patch -p1 -i ../newlib.patch

# Install libc headers before building the cross-compiler
# TODO: probably we just could create a dummy limits.h file
install-headers: $(NEWLIB) $(SYSROOT)
	cp -ar $(NEWLIB)/newlib/libc/include/* $(SYSROOT)/usr/include/
	cp -ar $(NEWLIB)/newlib/libc/sys/osdev/include/* $(SYSROOT)/usr/include/

$(NEWLIB)/newlib/libc/sys/osdev/%: lib/osdev/%
	@mkdir -p $(@D)
	cp $^ $@

$(NEWLIB)/newlib/Makefile.in: $(NEWLIB)/newlib/libc/sys/osdev/Makefile.inc
	$(V)cd $(@D) && autoreconf

$(OBJ)/lib/Makefile: $(NEWLIB)/newlib/Makefile.in
	$(V)rm -rf $(@D)
	$(V)mkdir -p $(@D)
	$(V)cd $(@D) && CFLAGS="$(LIB_CFLAGS)" ../../lib/newlib-4.4.0.20231231/configure \
		--host=arm-none-osdev \
		--target=arm-none-osdev \
		--prefix=/usr \
		--with-newlib \
		--disable-multilib

$(SYSROOT)/usr/lib/libc.a: $(OBJ)/lib/Makefile $(LIB_SRCFILES)
	$(V)cp -ar lib/osdev $(NEWLIB)/newlib/libc/sys/
	$(V)cd $(OBJ)/lib && make CFLAGS="$(LIB_CFLAGS)" DESTDIR=/$(HOME)/osdev/sysroot all install
	$(V)cp -ar $(SYSROOT)/usr/arm-none-osdev/* $(SYSROOT)/usr/

clean-lib:
	rm -rf $(OBJ)/lib
