#include <sys/time.h>		/* for setitimer */
#include <unistd.h>		/* for pause */
#include <signal.h>		/* for signal */
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#define INTERVAL 1000		/* number of milliseconds to go off */

/* function prototype */
void DoStuff(int code, siginfo_t *info, void *ctx);

int main(void) {
  struct sigaction sa;
  sa.sa_mask = 0;
  sa.sa_flags = 0;
  sa.sa_sigaction = DoStuff;
  struct itimerval it_val;	/* for setting itimer */

  /* Upon SIGALRM, call DoStuff().
   * Set interval timer.  We want frequency in ms, 
   * but the setitimer call needs seconds and useconds. */
  if (sigaction(SIGALRM, &sa, NULL) != 0) {
    perror("Unable to catch SIGALRM");
    exit(1);
  }
  it_val.it_value.tv_sec =     INTERVAL/1000;
  it_val.it_value.tv_usec =    (INTERVAL*1000) % 1000000;	
  it_val.it_interval = it_val.it_value;
  if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
    perror("error calling setitimer()");
    exit(1);
  }

  while (1) {}

  return 0;
}

/*
 * DoStuff
 */
void DoStuff(int code, siginfo_t *info, void *ctx) {
  ucontext_t *uc = (ucontext_t *) ctx;
  printf("Timer went off %d %d %p\n", code, info->si_signo, uc);
}
