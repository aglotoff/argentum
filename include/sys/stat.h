#ifndef __SYS_STAT_H__
#define __SYS_STAT_H__

#include <sys/types.h>

struct stat {
  mode_t  st_mode;    ///< File mode
  ino_t   st_ino;     ///< File serial number
  dev_t   st_dev;     ///< ID of device containing this file
  nlink_t st_nlink;   ///< Number of links
  uid_t   st_uid;     ///< User ID of the file's owner
  gid_t   st_gid;     ///< Group ID of the file's owner
  off_t   st_size;    ///< File size in bytes (only for regular files)
  time_t  st_atime;   ///< Time of last access
  time_t  st_mtime;   ///< Time of last data modification
  time_t  st_ctime;   ///< Time of last file status change
};

// The following values MUST equal to the corresponding values for the Ext2 FS
#define S_IFMT    (0xF << 12)   ///< File type
#define S_IFBLK   (0x6 << 12)   ///< Block special
#define S_IFCHR   (0x3 << 12)   ///< Character special
#define S_IFIFO   (0x1 << 12)   ///< FIFO special
#define S_IFREG   (0x8 << 12)   ///< Regular
#define S_IFDIR   (0x4 << 12)   ///< Directory
#define S_IFLNK   (0xA << 12)   ///< Symbolic link
#define S_IFSOCK  (0xC << 12)   ///< Socket

// File mode bits
#define S_IRUSR   (1 << 8)
#define S_IWUSR   (1 << 7)
#define S_IXUSR   (1 << 6)
#define S_IRWXU   (S_IRUSR | S_IWUSR | S_IXUSR)
#define S_IRGRP   (1 << 5)
#define S_IWGRP   (1 << 4)
#define S_IXGRP   (1 << 3)
#define S_IRWXG   (S_IRGRP | S_IWGRP | S_IXGRP)
#define S_IROTH   (1 << 2)
#define S_IWOTH   (1 << 1)
#define S_IXOTH   (1 << 0)
#define S_IRWXO   (S_IROTH | S_IWOTH | S_IXOTH)
#define S_ISUID   (1 << 11)
#define S_ISGID   (1 << 10)
#define S_ISVTX   (1 << 9)

#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISIFO(m)  (((m) & S_IFMT) == S_IFIFO)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

int fstat(int, struct stat *);
int mkdir(const char *, mode_t);
int mknod(const char *path, mode_t, dev_t);
int stat(const char *, struct stat *);

#ifdef __cplusplus
};
#endif  // __cplusplus

#endif  // !__SYS_STAT_H__
