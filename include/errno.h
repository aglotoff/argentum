#ifndef __ERRNO_H__
#define __ERRNO_H__

/**
 * @file include/errno.h
 * 
 * System error numbers.
 */

/**
 * Number of the last error.
 */
extern int errno;

#define E2BIG         1     ///< Arg list too long
#define EACCES       2     ///< Permission denied
#define EAGAIN        3     ///< Resource temporarily unavailable
#define EBADF         4     ///< Bad file descriptor
#define EBUSY         5     ///< Resource busy
#define ECHILD        6     ///< No child processes
#define EDEADLK       7     ///< Resource deadlock avoided
#define EDOM          8     ///< Domain error
#define EEXIST        9     ///< File exists
#define EFAULT        10    ///< Bad address
#define EFBIG         11    ///< File too large
#define EINTR         12    ///< Interrupted function call
#define EINVAL        13    ///< Invalid argument
#define EIO           14    ///< Input/output error
#define EISDIR        15    ///< Is a directory
#define EMFILE        16    ///< Too many open files
#define EMLINK        17    ///< Too many links
#define ENAMETOOLONG  18    ///< Filename too long
#define ENFILE        19    ///< Too many open files in system
#define ENODEV        20    ///< No such device
#define ENOENT        21    ///< No such file or directory
#define ENOEXEC       22    ///< Exec format error
#define ENOLCK        23    ///< No locks available
#define ENOMEM        24    ///< Not enough space
#define ENOSPC        25    ///< No space left on device
#define ENOSYS        26    ///< Function not implemented
#define ENOTDIR       27    ///< Not a directory
#define ENOTEMPTY     28    ///< Directory not empty
#define ENOTTY        29    ///< Inappropriate I/O control operation
#define ENXIO         30    ///< No such device or address
#define EOPNOTSUPP    31
#define EOVERFLOW     32
#define EPERM         33    ///< Operation not permitted
#define EPIPE         34    ///< Broken pipe
#define ERANGE        35    ///< Result too large
#define EROFS         36    ///< Read-only file system
#define ESPIPE        37    ///< Invalid seek
#define ESRCH         38    ///< No such process
#define EXDEV         39    ///< Improper link
#define ELOOP         40    ///< Too many levels of symbolic links

#endif  // !__ERRNO_H__
