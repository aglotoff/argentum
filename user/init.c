#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(void)
{
  int status;
  
  if (fork() == 0) {
    exec("/hello");
  }

  while (wait(&status) > 0)
    ;

  return 0;
}
