#define _POSIX_SOURCE
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h> /*FIX: used to be <wait.h>*/
#include <stdlib.h>

int
main(void) {
  for (;;);
  return 0;
}
