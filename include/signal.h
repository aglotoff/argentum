#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#define SIG_DFL   ((void (*)(void)) -1)
#define SIG_IGN   ((void (*)(void)) -2)

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


/** Used to represent sets of signals */
typedef unsigned long sigset_t;

struct sigaction {
  void   (*sa_handler)(void);
  sigset_t sa_mask;
  int      sa_flags;
};

#endif  // !__SIGNAL_H__
