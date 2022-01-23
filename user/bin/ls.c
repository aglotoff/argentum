#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char buf[1024];

int
main(int argc, char **argv)
{
  ssize_t nread;
  int fd;
  char *dirname;

  dirname = argc < 2 ? "." : argv[1];

  if ((fd = open(dirname, O_RDONLY)) < 0) {
    perror(dirname);
    exit(EXIT_FAILURE);
  }

  while ((nread = getdents(fd, buf, sizeof(buf))) != 0) {
    char *p;

    if (nread < 0) {
      perror(dirname);
      exit(EXIT_FAILURE);
    }

    p = buf;
    while (p < &buf[nread]) {
      struct dirent *dp = (struct dirent *) p;
      printf("%*.s\n", dp->d_namelen, dp->d_name);
      p += dp->d_reclen;
    }
  }

  return 0;
}
