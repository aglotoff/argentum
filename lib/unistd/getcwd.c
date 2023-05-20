#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char dbuf[10240];
char name[PATH_MAX];

struct Entry {
  struct Entry *next;
  char *name;
};

static void
free_entries(struct Entry *e)
{
  while (e != NULL) {
    struct Entry *prev;
    
    prev = e;
    e = e->next;
  
    if (prev->name)
      free(prev->name);
    free(prev);
  }
}

char
*getcwd(char *buf, size_t size)
{
  struct stat st;
  dev_t curr_dev;
  ino_t curr_ino;
  int fd;
  struct Entry *path;
  char *s;

  if (size < 2) {
    errno = (size == 0) ? EINVAL : ERANGE;
    return NULL;
  }

  strcpy(name, ".");
  if (stat(name, &st) != 0)
    return NULL;

  curr_dev = st.st_dev;
  curr_ino = st.st_ino;
  path = NULL;

  for (;;) {
    dev_t parent_dev;
    ino_t parent_ino;
    ssize_t nread;
    struct Entry *next;

    strcat(name, "/..");
    if ((fd = open(name, O_RDONLY)) < 0)
      return NULL;

    if (fstat(fd, &st) != 0) {
      close(fd);
      return NULL;
    }

    parent_dev = st.st_dev;
    parent_ino = st.st_ino;

    if ((curr_dev == parent_dev) && (curr_ino == parent_ino)) {
      close(fd);
      break;
    }

    next = NULL;
    while ((next == NULL) && (nread = getdents(fd, dbuf, sizeof(dbuf))) != 0) {
      char *p;

      if (nread < 0) {
        close(fd);
        return NULL;
      }

      p = dbuf;
      while (p < &dbuf[nread]) {
        struct dirent *dp = (struct dirent *) p;

        if (dp->d_ino == curr_ino) {
          if (((next = malloc(sizeof(struct Entry))) == NULL) ||
              ((next->name = malloc(strlen(dp->d_name) + 1)) == NULL)) {

            if (next != NULL)
              free(next);
            free_entries(path);

            close(fd);
            errno = ENOMEM;
            return NULL;
          }

          strcpy(next->name, dp->d_name);

          next->next = path;
          path = next;

          break;
        }

        p += dp->d_reclen;
      }
    }

    close(fd);

    if (next == NULL) {
      free_entries(path);
      errno = EACCES;
      return NULL;
    }

    curr_dev = parent_dev;
    curr_ino = parent_ino;
  }

  s = buf;

  if (path == NULL) {
    strcpy(s, "/");
    return buf;
  }

  while (path != NULL) {
    if ((strlen(path->name) + 1) >= size) {
      errno = ERANGE;
      free_entries(path);
      return NULL;
    }

    size -= strlen(path->name) + 1;

    s += snprintf(s, size, "/%s", path->name);
    path = path->next;
  }

  free_entries(path);

  return buf;
}