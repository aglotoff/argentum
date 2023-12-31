#ifndef __AG_INCLUDE_SYS_TYPES_H__
#define __AG_INCLUDE_SYS_TYPES_H__

/** Represents system times in clock ticks or CLOCKS_PER_SEC. */
typedef long long       clock_t;
/** Represents clock ID type in the clock and timer functions. */
typedef int             clockid_t;
/** Represents device IDs. */
typedef short           dev_t;
/** Represents group IDs. */
typedef short           gid_t;
/** Represents file serial numbers. */
typedef unsigned long   ino_t;
/** Represents some file attributes. */
typedef unsigned short  mode_t;
/** Represents link counts. */
typedef short           nlink_t;
/** Represents file sizes. */
typedef long            off_t;
/** Represents process IDs and process group IDs. */
typedef int             pid_t;
/** Represents sizes of objects. */
typedef unsigned long   size_t;
/** Represents a count of bytes or an error indication. */
typedef long            ssize_t;
/** Represents time in microseconds. */
typedef long            suseconds_t;
/** Represents time in seconds.*/
typedef long            time_t;
/** Represents user IDs. */
typedef short           uid_t;

#endif  // !__AG_INCLUDE_SYS_TYPES_H__
