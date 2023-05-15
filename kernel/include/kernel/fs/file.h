#ifndef __KERNEL_INCLUDE_KERNEL_FS_FILE_H__
#define __KERNEL_INCLUDE_KERNEL_FS_FILE_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <sys/types.h>

struct Inode;
struct stat;

#define FD_INODE    0
#define FD_PIPE     1

struct File {
  int           type;         ///< File type (inode, console, or pipe)
  int           ref_count;    ///< The number of references to this file
  int           readable;     ///< Whether the file is readable?
  int           writeable;    ///< Whether the file is writeable?
  off_t         offset;       ///< Current offset within the file
  struct Inode *inode;        ///< Pointer to the corresponding inode
};

void         file_init(void);
int          file_open(const char *, int, mode_t, struct File **);
struct File *file_dup(struct File *);
void         file_close(struct File *);
ssize_t      file_read(struct File *, void *, size_t);
ssize_t      file_write(struct File *, const void *, size_t);
ssize_t      file_getdents(struct File *, void *, size_t);
int          file_stat(struct File *, struct stat *);
int          file_chdir(struct File *);

#endif  // !__KERNEL_INCLUDE_KERNEL_FS_FILE__
