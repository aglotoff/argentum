#include <signal.h>
#include <stdio.h>
#include <ucontext.h>

volatile int fff = 0;

void
handler(int signo, siginfo_t *info, void *context) {
  printf("Helllooooo! %d, [%d %d %d %lx]\n", 
          signo, 
          info->si_code, 
          info->si_signo, 
          info->si_value.sival_int,
          ((ucontext_t *) context)->uc_mcontext.pc);
}

int main(void) {
  struct sigaction sa;
  sa.sa_sigaction = handler;

  sigaction(SIGINT, &sa, NULL);

   while (fff == 0) { 

   }

   printf("deddddd\n");

   return 0;
}
