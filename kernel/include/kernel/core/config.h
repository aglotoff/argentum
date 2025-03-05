#ifndef _KERNEL_CORE_CONFIG_H_
#define _KERNEL_CORE_CONFIG_H_

#include <stddef.h>         // NULL, size_t, offsetof
#include <stdint.h>         // integer types

#include <errno.h>          // error codes
#include <limits.h>         // NZERO
#include <string.h>         // memmove

#include <kernel/console.h> // _panic, _warn

#define K_TASK_MAX_PRIORITIES  (2 * NZERO)

#define K_ERR_AGAIN   (-EAGAIN)
#define K_ERR_DEADLK  (-EDEADLK)
#define K_ERR_INVAL   (-EINVAL)

#define k_memmove     memmove
#define k_panic(...)  _panic(__FILE__, __LINE__, __VA_ARGS__)
#define k_warn(...)   _warn(__FILE__, __LINE__, __VA_ARGS__)

#endif  // !_KERNEL_CORE_CONFIG_H_
