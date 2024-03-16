#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char name[256];
static char datebuf[256];

int
main(int argc, char **argv)
{
  DIR *dir;
  char *dirname;
  struct dirent *de;

  dirname = argc < 2 ? "." : argv[1];

  if ((dir = opendir(dirname)) == NULL) {
    perror(dirname);
    exit(EXIT_FAILURE);
  }

  while ((de = readdir(dir)) != NULL) {
    struct stat st;
    const char *color;

    // TODO: check boundaries!
    strcat(name, dirname);
    strcat(name, "/");
    strcat(name, de->d_name);

    if (stat(name, &st) < 0) {
      perror(name);
      closedir(dir);
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
    printf("%2d %d %d ", st.st_uid, st.st_gid, st.st_nlink);

    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
      printf("%3d,%3d", (st.st_rdev >> 8) & 0xFF, st.st_rdev & 0xFF);
    } else {
      printf("%7ld", st.st_size);
    }

    strftime(datebuf, 256, "%b %d %H:%M", gmtime(&st.st_mtime));
    printf(" %s", datebuf);
    printf(" \x1b[%sm%s\x1b[m\n", color, de->d_name);

    name[0] = '\0';
  }

  if (errno) {
    perror(dirname);
    closedir(dir);
    exit(EXIT_FAILURE);
  }

  closedir(dir);

  return 0;
}
