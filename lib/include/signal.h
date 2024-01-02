#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#define SIG_DFL   ((void (*)(void)) -1)
#define SIG_IGN   ((void (*)(void)) -2)
#define SIG_ERR   ((void (*)(void)) -3)

#define SIGABRT   1     ///< Abnormal termination
#define SIGALRM   2     ///< Timeout
#define SIGFPE    3     ///< Erroneous arithmetic operation
#define SIGHUP    4     ///< Hangup on controlling terminal
#define SIGILL    5     ///< Invalid hardware instruction
#define SIGINT    6     ///< Interactive attention
#define SIGKILL   7     ///< Termination
#define SIGPIPE   8     ///< Write on a pipe with no readers
#define SIGQUIT   9     ///< Interactive termination
#define SIGSEGV   10    ///< Invalid memory reference
#define SIGTERM   11    ///< Termination
#define SIGUSR1   12    ///< Application-defined signal 1
#define SIGUSR2   13    ///< Application-defined signal 2
#define SIGCHLD   14    ///< Child process terminated or stopped
#define SIGCONT   15    ///< Continue if stopped
#define SIGSTOP   16    ///< Stop signal
#define SIGTSTP   17    ///< Interactive stop signal
#define SIGTTIN   18    ///< Background process wants to read
#define SIGTTOU   19    ///< Background process wants to write
#define SIGBUS    20    ///< Access to an undefined portion of a memory object
#define SIGPOLL   21    ///< Pollable event
#define SIGPROF   22    ///< Profiling timer expired
#define SIGSYS    23    ///< Bad system call
#define SIGTRAP   24    ///< Trace/breakpoint trap
#define SIGURG    25    ///< High bandwidth data is available at a socket
#define SIGVTALRM 26    ///< Virtual timer expired
#define SIGXCPU   27    ///< CPU time limit exceeded
#define SIGXFSZ   28    ///< File size limit exceeded

#define SIG_BLOCK 0
#define SIG_SETMASK 1

/** Used to represent sets of signals */
typedef unsigned long sigset_t;

struct sigaction {
  void   (*sa_handler)(void);
  sigset_t sa_mask;
  int      sa_flags;
};

int raise(int);
int sigaction(int, const struct sigaction *, struct sigaction *);
void (*signal(int, void (*)(int)))(int);
int sigprocmask(int, const sigset_t *, sigset_t *);
int sigemptyset(sigset_t *);
int sigismember(const sigset_t *, int);
int sigaddset(sigset_t *, int);

#endif  // !__SIGNAL_H__
