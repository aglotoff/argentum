#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE  1024
char buf[BUF_SIZE];

int
main(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i++) {
    ssize_t nread;
    int fd;

    if ((fd = open(argv[i], O_RDONLY)) < 0) {
      perror(argv[i]);
      exit(EXIT_FAILURE);
    }

    while ((nread = read(fd, buf, BUF_SIZE)) != 0) {
      if (nread < 0) {
        perror(argv[i]);
        close(fd);
        exit(EXIT_FAILURE);
      }

      if ((write(1, buf, nread)) != nread) {
        perror(argv[i]);
        close(fd);
        exit(EXIT_FAILURE);
      }
    }

    close(fd);
  }

  return 0;
}