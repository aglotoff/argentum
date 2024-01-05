#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

char buf[PATH_MAX + 1];

int
main(void)
{
  if (getcwd(buf, sizeof(buf)) == NULL) {
    perror("getcwd");
    exit(EXIT_FAILURE);
  }

  printf("%s\n", buf);

  return 0;
}

