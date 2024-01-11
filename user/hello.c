#include <fnmatch.h>
#include <stdio.h>
#include <signal.h>
#include <sys/syscall.h>

char *testp = NULL;
char buf[34];

void
handler(int sig)
{
  printf("Signal %d\n", sig);
}

int
test(int a, int b, int c, int d, int e, int f)
{
  return __syscall(__SYS_TEST, a, b, c, d, e, f);
}

int
main(void)
{
  struct sigaction act;
  (void) act;

  act.sa_handler = handler;
  act.sa_flags = 0;
  act.sa_mask = 0;

  test(20, 50, 60, 700, 980, 1100);

  sigaction(SIGSEGV, &act, NULL);

 
  *testp = 9;

  printf("ok\n");

  return 0;
}

