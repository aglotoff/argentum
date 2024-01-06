LIB_CFLAGS := -mcpu=cortex-a9 -mhard-float -mfpu=vfp -nostdlib

NEWLIB := lib/newlib-4.4.0.20231231
NEWLIB_TARBALL := tarballs/newlib-4.4.0.20231231.tar.gz

LIB_SRCFILES := \
	lib/osdev/fcntl/fcntl.c \
	lib/osdev/fcntl/open.c \
	lib/osdev/include/sys/dirent.h \
	lib/osdev/include/sys/resource.h \
	lib/osdev/include/sys/syscall.h \
	lib/osdev/include/sys/utime.h \
	lib/osdev/include/sys/utsname.h \
	lib/osdev/include/limits.h \
	lib/osdev/signal/kill.c \
	lib/osdev/signal/sigaction.c \
	lib/osdev/signal/sigprocmask.c \
	lib/osdev/sys/resource/getrlimit.c \
	lib/osdev/sys/stat/chmod.c \
	lib/osdev/sys/stat/fstat.c \
	lib/osdev/sys/stat/mkdir.c \
	lib/osdev/sys/stat/mknod.c \
	lib/osdev/sys/stat/stat.c \
	lib/osdev/sys/stat/umask.c \
	lib/osdev/sys/time/gettimeofday.c \
	lib/osdev/sys/utsname/uname.c \
	lib/osdev/sys/wait/wait.c \
	lib/osdev/sys/wait/waitpid.c \
	lib/osdev/unistd/_exit.c \
	lib/osdev/unistd/access.c \
	lib/osdev/unistd/chdir.c \
	lib/osdev/unistd/close.c \
	lib/osdev/unistd/dup2.c \
	lib/osdev/unistd/execve.c \
	lib/osdev/unistd/fork.c \
	lib/osdev/unistd/fsync.c \
	lib/osdev/unistd/getcwd.c \
	lib/osdev/unistd/getdents.c \
	lib/osdev/unistd/getpid.c \
	lib/osdev/unistd/link.c \
	lib/osdev/unistd/lseek.c \
	lib/osdev/unistd/pipe.c \
	lib/osdev/unistd/read.c \
	lib/osdev/unistd/rmdir.c \
	lib/osdev/unistd/sbrk.c \
	lib/osdev/unistd/sleep.c \
	lib/osdev/unistd/unlink.c \
	lib/osdev/unistd/write.c \
	lib/osdev/utime/utime.c \
	lib/osdev/crt0.S \
	lib/osdev/progname.c \

LIB_SRCFILES := $(patsubst lib/%, $(NEWLIB)/newlib/libc/sys/%, $(LIB_SRCFILES))

$(NEWLIB_TARBALL):
	@mkdir -p $(@D)
	$(V)wget -O $@ ftp://sourceware.org/pub/newlib/newlib-4.4.0.20231231.tar.gz

$(NEWLIB): $(NEWLIB_TARBALL)
	$(V)tar xf $^ -C $(@D)
	cp -ar lib/osdev $@/newlib/libc/sys/
	$(V)pushd $@ && patch -p1 -i ../newlib.patch

# Install libc headers before building the cross-compiler
# TODO: probably we just could create a dummy limits.h file
install-headers: $(NEWLIB) $(SYSROOT)
	cp -ar $(NEWLIB)/newlib/libc/include/* $(SYSROOT)/usr/include/
	cp -ar $(NEWLIB)/newlib/libc/sys/osdev/include/* $(SYSROOT)/usr/include/

$(NEWLIB)/newlib/libc/sys/osdev/%: lib/osdev/%
	@mkdir -p $(@D)
	cp $^ $@

$(NEWLIB)/newlib/Makefile.in: $(NEWLIB) $(NEWLIB)/newlib/libc/sys/osdev/Makefile.inc
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

all-lib: $(SYSROOT)/usr/lib/libc.a

clean-lib:
	rm -rf $(OBJ)/lib
