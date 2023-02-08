#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

char buf[10240];
char name[256];
char datebuf[256];

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
      close(fd);
      exit(EXIT_FAILURE);
    }

    p = buf;
    while (p < &buf[nread]) {
      struct dirent *dp = (struct dirent *) p;
      struct stat st;
      const char *color;

      // TODO: check boundaries!
      strcat(name, dirname);
      strcat(name, "/");
      name[strlen(name) + dp->d_namelen] = '\0';
      strncpy(name + strlen(name), dp->d_name, dp->d_namelen);

      if (stat(name, &st) < 0) {
        perror(name);
        close(fd);
        exit(EXIT_FAILURE);
      }

      if (S_ISDIR(st.st_mode)) {
        color = "1;34";
      } else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
        color = "1;33";
      } else if (st.st_mode & (S_IXGRP | S_IXUSR | S_IXUSR)) {
        color = "1;32";
      } else {
        color = "";
      }

      printf("%c%c%c%c%c%c%c%c%c%c ",
             st.st_mode & S_IFDIR ? 'd' : '-',
             st.st_mode & S_IRUSR ? 'r' : '-',
             st.st_mode & S_IWUSR ? 'w' : '-',
             st.st_mode & S_IXUSR ? 'x' : '-',
             st.st_mode & S_IRGRP ? 'r' : '-',
             st.st_mode & S_IWGRP ? 'w' : '-',
             st.st_mode & S_IXGRP ? 'x' : '-',
             st.st_mode & S_IROTH ? 'r' : '-',
             st.st_mode & S_IWOTH ? 'w' : '-',
             st.st_mode & S_IXOTH ? 'x' : '-');
      printf("%2d root root %6d", st.st_nlink, st.st_size);
      strftime(datebuf, 256, "%b %d %H:%M", gmtime(&st.st_mtime));
      printf(" %s", datebuf);
      printf(" \x1b[%sm%.*s\x1b[m\n", color, dp->d_namelen, dp->d_name);

      p += dp->d_reclen;

      name[0] = '\0';
    }
  }

  close(fd);

  return 0;
}
