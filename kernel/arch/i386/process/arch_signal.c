#include <errno.h>

#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/vmspace.h>
#include <kernel/console.h>

int
arch_signal_prepare(struct Process *process, struct SignalFrame *frame)
{
  // TODO
  (void) process;
  (void) frame;
  return -1;
}

int
arch_signal_return(struct Process *process, const struct SignalFrame *ctx)
{
  // TODO
  (void) process;
  (void) ctx;
  return -1;
}
