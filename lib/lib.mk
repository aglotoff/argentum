LIB_CFLAGS := -mcpu=cortex-a9 -mhard-float -mfpu=vfp

NEWLIB := lib/newlib-4.4.0.20231231

rebuild-lib:
	$(V)cp -ar lib/osdev $(NEWLIB)/newlib/libc/sys/
	$(V)pushd $(NEWLIB) && patch -p1 -i ../newlib.patch && cd newlib && autoreconf

lib/build-lib:
	$(V)mkdir -p $@
	$(V)pushd $@ && CFLAGS="$(LIB_CFLAGS)" ../newlib-4.4.0.20231231/configure \
		--host=arm-none-osdev \
		--target=arm-none-osdev \
		--prefix=/usr \
		--with-newlib \
		--disable-multilib && make all && make DESTDIR=/home/andreas/osdev/sysroot install
