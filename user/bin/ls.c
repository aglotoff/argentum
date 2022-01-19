#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct dirent {
  uint32_t inode;
  uint16_t rec_len;
  uint8_t  name_len;
  uint8_t  file_type;
  char     name[256];
} __attribute__((packed));

#define DIRENT_LEN  offsetof(struct dirent, name)

char buf[1024];

int
main(int argc, char **argv)
{
  struct dirent de;
  int fd;
  
  if (argc < 2) {
    printf("Usage: %s <dirname>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((fd = open(argv[1], O_RDONLY)) < 0) {
    perror(argv[1]);
    exit(EXIT_FAILURE);
  }

  while (read(fd, &de, DIRENT_LEN) == DIRENT_LEN) {
    if (read(fd, de.name, de.name_len) != de.name_len) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    printf("%.*s\n", de.name_len, de.name);

    if (read(fd, buf, de.rec_len - de.name_len - DIRENT_LEN) < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}
