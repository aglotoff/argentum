#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/wait.h>

int n = 10;

int main(void)
{
  int fd[2];
  ssize_t r;

  if (pipe(fd) < 0) {
    perror("pipe");
    exit(1);
  }

  if (fork() == 0) {
    char buf[128];

    close(0);
    dup(fd[0]);

    close(fd[0]);
    close(fd[1]);

    if ((r = read(0, buf, sizeof(buf))) < 0) {
      perror("read");
      exit(1);
    }

    buf[r] = '\0';
    printf("Got: %s\n", buf);
  } else {
    close(1);
    dup(fd[1]);

    close(fd[0]);
    close(fd[1]);

    if ((r = write(1, "djhhdfdfjhfd5", 14)) < 0) {
      perror("write");
      exit(1);
    }
  }

  return 0;
}
