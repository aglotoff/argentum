#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE  1024
char buf[BUF_SIZE];

int
main(int argc, char **argv)
{
  ssize_t nread;
  int fd, i;

  if (argc < 2) {
    while ((nread = read(0, buf, BUF_SIZE)) != 0) {
      if ((nread < 0) || (write(1, buf, nread) != nread)) {
        perror(argv[0]);
        exit(EXIT_FAILURE);
      }
    }
  } else {
    for (i = 1; i < argc; i++) {
      if ((fd = open(argv[i], O_RDONLY)) < 0) {
        perror(argv[i]);
        exit(EXIT_FAILURE);
      }

      while ((nread = read(fd, buf, BUF_SIZE)) != 0) {
        if ((nread < 0) || (write(1, buf, nread) != nread)) {
          perror(argv[i]);
          close(fd);
          exit(EXIT_FAILURE);
        }
      }

      close(fd);
    }
  }

  return 0;
}