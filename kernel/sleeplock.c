#include <assert.h>

#include "process.h"
#include "sleeplock.h"
#include "spinlock.h"

void
sleep_init(struct Sleeplock *lock, const char *name)
{
  spin_init(&lock->lock, "sleeplock");
  list_init(&lock->wait_queue.head);
  lock->process = NULL;
  lock->name = name;
}

void
sleep_lock(struct Sleeplock *lock)
{
  spin_lock(&lock->lock);

  while (lock->process != NULL)
    process_sleep(&lock->wait_queue, &lock->lock);
  
  lock->process = my_process();

  spin_unlock(&lock->lock);
}

void
sleep_unlock(struct Sleeplock *lock)
{
  if (!sleep_holding(lock))
    panic("not holding");
  
  spin_lock(&lock->lock);

  lock->process = NULL;
  process_wakeup(&lock->wait_queue);

  spin_unlock(&lock->lock);
}

int
sleep_holding(struct Sleeplock *lock)
{
  struct Process *process;

  spin_lock(&lock->lock);
  process = lock->process;
  spin_unlock(&lock->lock);

  return (process != NULL) && (process == my_process());
}
