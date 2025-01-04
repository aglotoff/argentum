#include <stdlib.h>

char *__progname = {'\0'};

extern char **environ;

void __libc_init_array(void);
int  main(int, char **, char **);

#if defined(__i386__)
void _init(void) {}
void _fini(void) {}
#endif

void
_start(int argc, char **argv, char **envp)
{
  int r;

#if defined(__arm__) || defined(__thumb__)
  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  asm volatile("mov r11, #0");
#endif

#if defined(__i386__)
  // Clear the frame pointer register (EBP) so that stack backtraces will be
  // terminated properly.
  asm volatile("movl $0,%ebp");
#endif

  if ((argc > 0) && (argv[0] != NULL))
    __progname = argv[0];

  if (envp != NULL)
    environ = envp;

  __libc_init_array();

  r = main(argc, argv, envp);

  exit(r);
}
