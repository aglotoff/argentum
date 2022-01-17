#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(void)
{
  int status;

  char *const argv[] = {
    "This is",
    "test",
    NULL
  };
  
  if (fork() == 0)
    exec("/hello", argv);

  while (wait(&status) > 0)
    ;

  return 0;
}
