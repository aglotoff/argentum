#ifndef __SYS_UTSNAME_H__
#define __SYS_UTSNAME_H__

/**
 * @file include/sys/utsname.h
 * 
 * System name structure.
 */

#define __UTSNAME_LENGTH  65

/**
 * System name structure.
 */
struct utsname {
  /**
   * Name of this implementation of the operating system.
   */
  char sysname[__UTSNAME_LENGTH];
  /**
   * Name of this node within the communications network to which this node is
   * attached, if any.
   */
  char nodename[__UTSNAME_LENGTH];
  /**
   * Current release level of this implementation.
   */
  char release[__UTSNAME_LENGTH];
  /**
   * Current version level of this release.
   */
  char version[__UTSNAME_LENGTH];
  /**
   * Name of the hardware type on which the system is running.
   */
  char machine[__UTSNAME_LENGTH];
};

int uname(struct utsname *);

#endif  // !__SYS_UTSNAME_H__
