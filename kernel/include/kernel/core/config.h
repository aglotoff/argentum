#ifndef _KERNEL_CORE_CONFIG_H_
#define _KERNEL_CORE_CONFIG_H_

#include <stddef.h>         // NULL, size_t, offsetof
#include <stdint.h>         // integer types

#include <errno.h>          // error codes
#include <limits.h>         // NZERO
#include <string.h>         // memmove

#include <kernel/console.h> // _panic, _warn

// TODO: should be architecture-specific
#define K_CPU_MAX   4

#define K_TASK_MAX_PRIORITIES  (2 * NZERO)

#define K_ERR_AGAIN   (-EAGAIN)
#define K_ERR_DEADLK  (-EDEADLK)
#define K_ERR_INVAL   (-EINVAL)

#define k_memmove     memmove
#define k_panic(...)  _panic(__FILE__, __LINE__, __VA_ARGS__)
#define k_warn(...)   _warn(__FILE__, __LINE__, __VA_ARGS__)

struct KTask;

void on_task_destroy(struct KTask *);
void on_task_before_switch(struct KTask *);
void on_task_after_switch(struct KTask *);
void on_task_idle(void);

#define K_ON_TASK_DESTROY         on_task_destroy
#define K_ON_TASK_BEFORE_SWITCH   on_task_before_switch
#define K_ON_TASK_AFTER_SWITCH    on_task_after_switch
#define K_ON_TASK_IDLE            on_task_idle

#endif  // !_KERNEL_CORE_CONFIG_H_
