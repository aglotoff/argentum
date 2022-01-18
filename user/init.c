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

  printf("Welcome to \x1b[1;36mOSDev-PBX-A9!\x1b[m\n");
  
  if (fork() == 0)
    exec("/bin/sh", argv);

  while (wait(&status) > 0)
    ;

  return 0;
}
