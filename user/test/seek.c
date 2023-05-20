#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int
main(void)
{
  extern char *__progname;

  printf("%s\n", __progname);

  int fd = open("./file.txt", O_RDWR | O_CREAT | O_TRUNC);

  write(fd, "ABABABABABABA", 13);
  lseek(fd, -7, SEEK_CUR);
  write(fd, "FFFFF", 5);

  lseek(fd, 3045, SEEK_CUR);
  write(fd, "LOL", 3);
  lseek(fd, 4, SEEK_END);
  write(fd, "LOLEDD", 6);
  lseek(fd, 100, SEEK_SET);
  write(fd, "RORORORORO", 10);

  return 0;
}
