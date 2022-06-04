#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>

int
main(void)
{
  struct utsname name;

  if (uname(&name) < 0) {
    perror("uname");
    exit(EXIT_FAILURE);
  }

  printf("%s %s %s %s %s\n", name.sysname, name.nodename, name.release,
         name.version, name.machine);
  
  return 0;
}
