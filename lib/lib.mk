#
# Makefile fragment for the library
#

LIB_CFLAGS := $(CFLAGS)

LIB_SRCFILES := 

LIB_SRCFILES += \
  lib/assert/__panic.c

LIB_SRCFILES += \
  lib/ctype/__ctype.c \
	lib/ctype/__tolower.c \
	lib/ctype/__toupper.c \
  lib/ctype/isalnum.c \
	lib/ctype/isalpha.c \
	lib/ctype/iscntrl.c \
	lib/ctype/isdigit.c \
	lib/ctype/isgraph.c \
	lib/ctype/islower.c \
	lib/ctype/isprint.c \
	lib/ctype/ispunct.c \
	lib/ctype/isspace.c \
	lib/ctype/isupper.c \
	lib/ctype/isxdigit.c \
	lib/ctype/tolower.c \
	lib/ctype/toupper.c

LIB_SRCFILES += \
  lib/dirent/closedir.c \
	lib/dirent/fdopendir.c \
	lib/dirent/getdents.c \
	lib/dirent/opendir.c \
	lib/dirent/readdir.c \
	lib/dirent/rewinddir.c

LIB_SRCFILES += \
  lib/errno/errno.c

LIB_SRCFILES += \
  lib/fcntl/creat.c \
	lib/fcntl/fcntl.c \
	lib/fcntl/open.c

LIB_SRCFILES += \
	lib/float/__fvalues.c

LIB_SRCFILES += \
	lib/fnmatch/fnmatch.c

LIB_SRCFILES += \
	lib/locale/_C_locale.c \
	lib/locale/localeconv.c \
	lib/locale/setlocale.c

LIB_SRCFILES += \
	lib/math/__math_classify_double.c \
	lib/math/__math_log.c \
	lib/math/__math_normalize_double.c \
	lib/math/__math_scale_double.c \
	lib/math/__math_trunc_double.c \
	lib/math/__math_unscale_double.c \
	lib/math/ceil.c \
	lib/math/exp.c \
	lib/math/fabs.c \
	lib/math/floor.c \
	lib/math/fmod.c \
	lib/math/frexp.c \
	lib/math/ldexp.c \
	lib/math/log.c \
	lib/math/log10.c \
	lib/math/modf.c \
	lib/math/sqrt.c

LIB_SRCFILES += \
	lib/setjmp/longjmp.S \
	lib/setjmp/setjmp.S

LIB_SRCFILES += \
	lib/signal/raise.c \
	lib/signal/sigaction.c \
	lib/signal/sigaddset.c \
	lib/signal/sigemptyset.c \
	lib/signal/sigismember.c \
	lib/signal/signal.c \
	lib/signal/sigprocmask.c

LIB_SRCFILES += \
  lib/stdio/__fclose.c \
	lib/stdio/__fflush.c \
	lib/stdio/__ffree.c \
	lib/stdio/__files.c \
	lib/stdio/__fopen.c \
  lib/stdio/__printf.c \
	lib/stdio/fclose.c \
	lib/stdio/feof.c \
	lib/stdio/ferror.c \
	lib/stdio/fflush.c \
	lib/stdio/fgetc.c \
	lib/stdio/fgets.c \
	lib/stdio/fileno.c \
	lib/stdio/fopen.c \
	lib/stdio/fprintf.c \
	lib/stdio/fputc.c \
	lib/stdio/fputs.c \
	lib/stdio/fread.c \
	lib/stdio/freopen.c \
	lib/stdio/fseeko.c \
	lib/stdio/fsync.c \
	lib/stdio/ftell.c \
	lib/stdio/fwrite.c \
	lib/stdio/getc.c \
	lib/stdio/getchar.c \
	lib/stdio/gets.c \
	lib/stdio/perror.c \
	lib/stdio/printf.c \
	lib/stdio/putc.c \
	lib/stdio/putchar.c \
	lib/stdio/puts.c \
	lib/stdio/remove.c \
	lib/stdio/setbuf.c \
	lib/stdio/setvbuf.c \
	lib/stdio/snprintf.c \
	lib/stdio/sprintf.c \
	lib/stdio/ungetc.c \
	lib/stdio/vfprintf.c \
	lib/stdio/vprintf.c \
	lib/stdio/vsnprintf.c \
	lib/stdio/vsprintf.c

LIB_SRCFILES += \
  lib/stdlib/__getmem.c \
	lib/stdlib/__stdlib_parse_int.c \
	lib/stdlib/_Exit.c \
	lib/stdlib/abort.c \
	lib/stdlib/abs.c \
	lib/stdlib/atexit.c \
	lib/stdlib/atoi.c \
	lib/stdlib/atol.c \
	lib/stdlib/bsearch.c \
	lib/stdlib/calloc.c \
	lib/stdlib/div.c \
	lib/stdlib/environ.c \
	lib/stdlib/exit.c \
	lib/stdlib/free.c \
	lib/stdlib/getenv.c \
	lib/stdlib/labs.c \
	lib/stdlib/ldiv.c \
	lib/stdlib/malloc.c \
	lib/stdlib/qsort.c \
	lib/stdlib/rand.c \
	lib/stdlib/rand_r.c \
	lib/stdlib/realloc.c \
	lib/stdlib/setenv.c \
	lib/stdlib/srand.c \
	lib/stdlib/strtol.c \
	lib/stdlib/strtoul.c \
	lib/stdlib/wctomb.c

LIB_SRCFILES += \
	lib/string/memchr.c \
	lib/string/memcmp.c \
	lib/string/memcpy.c \
	lib/string/memmove.c \
	lib/string/memset.c \
	lib/string/strcat.c \
	lib/string/strchr.c \
	lib/string/strcmp.c \
	lib/string/strcpy.c \
	lib/string/strcspn.c \
	lib/string/strdup.c \
	lib/string/strerror.c \
	lib/string/strlen.c \
	lib/string/strncat.c \
	lib/string/strncmp.c \
	lib/string/strncpy.c \
	lib/string/strnlen.c \
	lib/string/strpbrk.c \
	lib/string/strrchr.c \
	lib/string/strspn.c \
	lib/string/strstr.c \
	lib/string/strtok.c

LIB_SRCFILES += \
  lib/sys/stat/chmod.c \
	lib/sys/stat/fstat.c \
	lib/sys/stat/mkdir.c \
	lib/sys/stat/mknod.c \
	lib/sys/stat/stat.c \
	lib/sys/stat/umask.c

LIB_SRCFILES += \
  lib/sys/time/gettimeofday.c

LIB_SRCFILES += \
	lib/sys/utsname/uname.c

LIB_SRCFILES += \
  lib/sys/wait/wait.c \
	lib/sys/wait/waitpid.c

LIB_SRCFILES += \
	lib/time/asctime.c \
	lib/time/clock.c \
	lib/time/clock_gettime.c \
	lib/time/difftime.c \
	lib/time/gmtime.c \
	lib/time/localtime.c \
	lib/time/mktime.c \
	lib/time/strftime.c \
	lib/time/time.c

LIB_SRCFILES += \
  lib/unistd/_exit.c \
	lib/unistd/access.c \
	lib/unistd/chdir.c \
	lib/unistd/close.c \
	lib/unistd/dup.c \
	lib/unistd/execl.c \
	lib/unistd/execle.c \
	lib/unistd/execlp.c \
	lib/unistd/execv.c \
	lib/unistd/execve.c \
	lib/unistd/execvp.c \
	lib/unistd/fchdir.c \
  lib/unistd/fork.c \
	lib/unistd/getcwd.c \
	lib/unistd/getgroups.c \
	lib/unistd/getpid.c \
	lib/unistd/getppid.c \
	lib/unistd/isatty.c \
	lib/unistd/link.c \
	lib/unistd/lseek.c \
	lib/unistd/pipe.c \
	lib/unistd/read.c \
	lib/unistd/rmdir.c \
	lib/unistd/sbrk.c \
	lib/unistd/sleep.c \
	lib/unistd/write.c \
	lib/unistd/unlink.c

LIB_SRCFILES += \
  lib/utime/utime.c \

LIB_OBJFILES := $(patsubst %.c, $(OBJ)/%.o, $(LIB_SRCFILES))
LIB_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(LIB_OBJFILES))

CRT_SRCFILES := \
	lib/crt0.S \
	lib/crti.S \
	lib/crtn.S \
	lib/crtbegin.S \
	lib/crtend.S

CRT_OBJFILES := $(patsubst %.S, $(OBJ)/%.o, $(CRT_SRCFILES))

$(OBJ)/lib/%.o: lib/%.c $(OBJ)/.vars.LIB_CFLAGS
	@echo "+ CC [LIB] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJ)/lib/%.o: lib/%.S $(OBJ)/.vars.LIB_CFLAGS
	@echo "+ AS [LIB] $<"
	@mkdir -p $(@D)
	$(V)$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJ)/lib/libc.a: $(LIB_OBJFILES)
	@echo "+ AR [LIB] $@"
	$(V)$(AR) r $@ $(LIB_OBJFILES)

lib: $(OBJ)/lib/libc.a $(CRT_OBJFILES)
