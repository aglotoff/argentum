#ifndef __KERNEL_FILE_H__
#define __KERNEL_FILE_H__

#include <sys/types.h>

struct Inode;

#define FD_INODE    0
#define FD_CONSOLE  1
#define FD_PIPE     2

struct File {
  int           type;         ///< File type (inode, console, or pipe)
  int           ref_count;    ///< The number of references to this file
  int           readable;     ///< Whether the file is readable?
  int           writeable;    ///< Whether the file is writeable?
  off_t         offset;       ///< Current offset within the file
  struct Inode *inode;        ///< Pointer to the corresponding inode
};

void         file_init(void);
int          file_open(const char *, int, struct File **);
struct File *file_dup(struct File *);
void         file_close(struct File *);
ssize_t      file_read(struct File *, void *, size_t);
ssize_t      file_write(struct File *, const void *, size_t);
ssize_t      file_getdents(struct File *, void *, size_t);

#endif  // !__KERNEL_FILE__
