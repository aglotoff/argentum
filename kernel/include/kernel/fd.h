#ifndef __KERNEL_FD_H__
#define __KERNEL_FD_H__

struct Process;
struct Channel;

void         fd_init(struct Process *);
void         fd_close_all(struct Process *);
void         fd_close_on_exec(struct Process *);
void         fd_clone(struct Process *, struct Process *);
int          fd_alloc(struct Process *, struct Channel *, int);
struct Channel *fd_lookup(struct Process *, int);
int          fd_close(struct Process *, int);
int          fd_get_flags(struct Process *, int);
int          fd_set_flags(struct Process *, int, int);

#endif  // !__KERNEL_FD_H__
