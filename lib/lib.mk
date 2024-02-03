LIB_CFLAGS := -nostdlib

NEWLIB := lib/newlib-4.4.0.20231231
NEWLIB_TARBALL := tarballs/newlib-4.4.0.20231231.tar.gz

LIB_SRCFILES := \
	lib/argentum/arpa/inet/inet_pton.c \
	lib/argentum/fcntl/fcntl.c \
	lib/argentum/fcntl/open.c \
	lib/argentum/grp/endgrent.c \
	lib/argentum/grp/getgrent.c \
	lib/argentum/grp/getgrgid.c \
	lib/argentum/grp/getgrnam.c \
	lib/argentum/grp/setgrent.c \
	lib/argentum/include/arpa/inet.h \
	lib/argentum/include/netinet/in.h \
	lib/argentum/include/sys/dirent.h \
	lib/argentum/include/sys/ioctl.h \
	lib/argentum/include/sys/resource.h \
	lib/argentum/include/sys/socket.h \
	lib/argentum/include/sys/syscall.h \
	lib/argentum/include/sys/termios.h \
	lib/argentum/include/sys/utime.h \
	lib/argentum/include/sys/utmp.h \
	lib/argentum/include/sys/utsname.h \
	lib/argentum/include/sys/vfs.h \
	lib/argentum/include/limits.h \
	lib/argentum/include/mntent.h \
	lib/argentum/include/netdb.h \
	lib/argentum/mntent/getmntent.c \
	lib/argentum/netdb/gethostbyname.c \
	lib/argentum/netdb/getservbyname.c \
	lib/argentum/signal/kill.c \
	lib/argentum/signal/killpg.c \
	lib/argentum/signal/sigaction.c \
	lib/argentum/signal/sigprocmask.c \
	lib/argentum/signal/sigstub.S \
	lib/argentum/signal/sigsuspend.c \
	lib/argentum/stdlib/realpath.c \
	lib/argentum/sys/resource/getrlimit.c \
	lib/argentum/sys/resource/setrlimit.c \
	lib/argentum/sys/socket/accept.c \
	lib/argentum/sys/socket/bind.c \
	lib/argentum/sys/socket/connect.c \
	lib/argentum/sys/socket/listen.c \
	lib/argentum/sys/socket/recv.c \
	lib/argentum/sys/socket/recvfrom.c \
	lib/argentum/sys/socket/send.c \
	lib/argentum/sys/socket/sendto.c \
	lib/argentum/sys/socket/setsockopt.c \
	lib/argentum/sys/socket/socket.c \
	lib/argentum/sys/stat/chmod.c \
	lib/argentum/sys/stat/fchmod.c \
	lib/argentum/sys/stat/fstat.c \
	lib/argentum/sys/stat/lstat.c \
	lib/argentum/sys/stat/mkdir.c \
	lib/argentum/sys/stat/mknod.c \
	lib/argentum/sys/stat/stat.c \
	lib/argentum/sys/stat/umask.c \
	lib/argentum/sys/time/gettimeofday.c \
	lib/argentum/sys/time/select.c \
	lib/argentum/sys/times/times.c \
	lib/argentum/sys/utsname/uname.c \
	lib/argentum/sys/vfs/statfs.c \
	lib/argentum/sys/wait/wait.c \
	lib/argentum/sys/wait/waitpid.c \
	lib/argentum/termios/cfgetispeed.c \
	lib/argentum/termios/cfgetospeed.c \
	lib/argentum/termios/cfsetispeed.c \
	lib/argentum/termios/cfsetospeed.c \
	lib/argentum/termios/tcflush.c \
	lib/argentum/termios/tcgetattr.c \
	lib/argentum/termios/tcsetattr.c \
	lib/argentum/time/nanosleep.c \
	lib/argentum/unistd/_exit.c \
	lib/argentum/unistd/access.c \
	lib/argentum/unistd/alarm.c \
	lib/argentum/unistd/chdir.c \
	lib/argentum/unistd/close.c \
	lib/argentum/unistd/dup.c \
	lib/argentum/unistd/dup2.c \
	lib/argentum/unistd/execve.c \
	lib/argentum/unistd/fchown.c \
	lib/argentum/unistd/fork.c \
	lib/argentum/unistd/fpathconf.c \
	lib/argentum/unistd/fsync.c \
	lib/argentum/unistd/ftruncate.c \
	lib/argentum/unistd/getdents.c \
	lib/argentum/unistd/getegid.c \
	lib/argentum/unistd/geteuid.c \
	lib/argentum/unistd/getgid.c \
	lib/argentum/unistd/getgroups.c \
	lib/argentum/unistd/getpgid.c \
	lib/argentum/unistd/getpgrp.c \
	lib/argentum/unistd/getpid.c \
	lib/argentum/unistd/getppid.c \
	lib/argentum/unistd/getuid.c \
	lib/argentum/unistd/lchown.c \
	lib/argentum/unistd/link.c \
	lib/argentum/unistd/lseek.c \
	lib/argentum/unistd/pathconf.c \
	lib/argentum/unistd/pipe.c \
	lib/argentum/unistd/read.c \
	lib/argentum/unistd/rmdir.c \
	lib/argentum/unistd/sbrk.c \
	lib/argentum/unistd/sethostname.c \
	lib/argentum/unistd/setpgid.c \
	lib/argentum/unistd/setregid.c \
	lib/argentum/unistd/setreuid.c \
	lib/argentum/unistd/sysconf.c \
	lib/argentum/unistd/tcgetpgrp.c \
	lib/argentum/unistd/tcsetpgrp.c \
	lib/argentum/unistd/unlink.c \
	lib/argentum/unistd/vfork.c \
	lib/argentum/unistd/write.c \
	lib/argentum/utime/utime.c \
	lib/argentum/crt0.S \
	lib/argentum/progname.c \

LIB_SRCFILES := $(patsubst lib/%, $(NEWLIB)/newlib/libc/sys/%, $(LIB_SRCFILES))

$(NEWLIB_TARBALL):
	@mkdir -p $(@D)
	$(V)wget -O $@ ftp://sourceware.org/pub/newlib/newlib-4.4.0.20231231.tar.gz

$(NEWLIB): $(NEWLIB_TARBALL)
	$(V)tar xf $^ -C $(@D)
	cp -ar lib/argentum $@/newlib/libc/sys/
	$(V)pushd $@ && patch -p1 -i ../newlib.patch

# Install libc headers before building the cross-compiler
# TODO: probably we just could create a dummy limits.h file
install-headers: $(NEWLIB) $(SYSROOT)
	cp -ar $(NEWLIB)/newlib/libc/include/* $(SYSROOT)/usr/include/
	cp -ar $(NEWLIB)/newlib/libc/sys/argentum/include/* $(SYSROOT)/usr/include/

$(NEWLIB)/newlib/libc/sys/argentum/%: lib/argentum/%
	@mkdir -p $(@D)
	cp $^ $@

$(NEWLIB)/newlib/Makefile.in: $(NEWLIB) $(NEWLIB)/newlib/libc/sys/argentum/Makefile.inc
	$(V)cd $(@D) && autoreconf

$(OBJ)/lib/Makefile: $(NEWLIB)/newlib/Makefile.in
	$(V)rm -rf $(@D)
	$(V)mkdir -p $(@D)
	$(V)cd $(@D) && CFLAGS="$(LIB_CFLAGS)" ../../lib/newlib-4.4.0.20231231/configure \
		--host=arm-none-argentum \
		--target=arm-none-argentum \
		--prefix=/usr \
		--with-newlib \
		--disable-multilib \
		--disable-newlib-nano-formatted-io \
		--enable-newlib-io-c99-formats

$(SYSROOT)/usr/lib/libc.a: $(OBJ)/lib/Makefile $(LIB_SRCFILES)
	$(V)cp -ar lib/argentum $(NEWLIB)/newlib/libc/sys/
	$(V)cd $(OBJ)/lib && make CFLAGS="$(LIB_CFLAGS)" DESTDIR=/$(HOME)/argentum/sysroot all install
	$(V)cp -ar $(SYSROOT)/usr/arm-none-argentum/* $(SYSROOT)/usr/

all-lib: $(SYSROOT)/usr/lib/libc.a

clean-lib:
	rm -rf $(OBJ)/lib
