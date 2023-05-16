#include <errno.h>
#include <string.h>

static char *messages[] = {
  [0]            = "No error",
  [E2BIG]        = "Arg list too long",
  [EACCES]      = "Permission denied",
  [EAGAIN]       = "Resource temporarily unavailable",
  [EBADF]        = "Bad file descriptor",
  [EBUSY]        = "Resource busy",
  [ECHILD]       = "No child processes",
  [EDEADLK]      = "Resource deadlock avoided",
  [EDOM]         = "Domain error",
  [EEXIST]      = "File exists",
  [EFAULT]       = "Bad address",
  [EFBIG]        = "File too large",
  [EINTR]        = "Interrupted function call",
  [EINVAL]       = "Invalid argument",
  [EIO]          = "Input/output error",
  [EISDIR]       = "Is a directory",
  [EMFILE]       = "Too many open files",
  [EMLINK]       = "To many links",
  [ENAMETOOLONG] = "Filename too long",
  [ENFILE]       = "Too many open files in system",
  [ENODEV]       = "No such device",
  [ENOENT]       = "No such file or directory",
  [ENOEXEC]      = "Exec format error",
  [ENOLCK]       = "No locks available",
  [ENOMEM]       = "Not enough space",
  [ENOSPC]       = "No space left on device",
  [ENOSYS]       = "Function not implemented",
  [ENOTDIR]      = "Not a directory",
  [ENOTEMPTY]    = "Directory not empty",
  [ENOTTY]       = "Inappropriate I/O control operation",
  [ENXIO]        = "No such device or address",
  [EPERM]        = "Operation not permitted",
  [EPIPE]        = "Broken pipe",
  [ERANGE]       = "Result too large",
  [EROFS]        = "Read-only file system",
  [ESPIPE]       = "Invalid seek",
  [ESRCH]        = "No such process",
  [EXDEV]        = "Improper link",
};

#define ERRMAX  ((int) ((sizeof messages) / (sizeof messages[0])))

/**
 * Get error message string.
 * 
 * Maps the error number in errnum to an error message and returns a pointer to
 * it.
 *
 * @param errnum The error number.
 *
 * @return A pointer to the generated message string.
 */
char *
strerror(int errnum)
{
  if ((errnum < 0) || (errnum > ERRMAX) || (messages[errnum] == NULL)) {
    errno = EINVAL;
    return "Unknown error";
  }

  return messages[errnum];
};
