#include <kernel/assert.h>
#include <errno.h>

#include <kernel/cprintf.h>
#include <kernel/timer.h>
#include <kernel/spinlock.h>

static void k_timer_enqueue(struct KTimer *);
static void k_timer_dequeue(struct KTimer *);

static KLIST_DECLARE(k_timer_queue);
static struct KSpinLock k_timer_queue_lock = K_SPINLOCK_INITIALIZER("timer");
static struct KTimer *k_timer_current;

int
k_timer_init(struct KTimer *timer,
             void (*callback)(void *), void *callback_arg,
             unsigned long delay, unsigned long period,
             int autostart)
{
  if (timer == NULL)
    panic("timer is NULL");

  k_list_null(&timer->link);
  timer->callback = callback;
  timer->callback_arg = callback_arg;
  timer->remain = 0;
  timer->delay  = delay;
  timer->period = period;
  timer->state = K_TIMER_STATE_INACTIVE;

  // TODO: validate delay and period!

  if (autostart) {
    k_spinlock_acquire(&k_timer_queue_lock);
    timer->remain = delay;
    k_timer_enqueue(timer);
    k_spinlock_release(&k_timer_queue_lock);
  }

  return 0;
}

void
_k_timer_start(struct KTimer *timer, unsigned long remain)
{
  k_spinlock_acquire(&k_timer_queue_lock);

  timer->remain = remain;
  k_timer_enqueue(timer);

  k_spinlock_release(&k_timer_queue_lock);
}

int
k_timer_start(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  k_spinlock_acquire(&k_timer_queue_lock);

  if (timer->state != K_TIMER_STATE_INACTIVE) {
    k_spinlock_release(&k_timer_queue_lock);
    return -EINVAL;
  }

  timer->remain = timer->delay;
  k_timer_enqueue(timer);

  k_spinlock_release(&k_timer_queue_lock);

  return 0;
}

int
k_timer_stop(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  k_spinlock_acquire(&k_timer_queue_lock);

  if (timer->state == K_TIMER_STATE_RUNNING) {
    assert(k_timer_current == timer);
    k_timer_current = NULL;
  } else if (timer->state == K_TIMER_STATE_ACTIVE) {
    k_timer_dequeue(timer);
  } else {
    // Invalid state
    k_spinlock_release(&k_timer_queue_lock);
    return -EINVAL;
  }

  timer->state = K_TIMER_STATE_INACTIVE;

  k_spinlock_release(&k_timer_queue_lock);

  return 0;
}

int
k_timer_fini(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  k_spinlock_acquire(&k_timer_queue_lock);

  switch (timer->state) {  
  case K_TIMER_STATE_ACTIVE:
    k_timer_dequeue(timer);
    break;
  case K_TIMER_STATE_INACTIVE:
    break;
  case K_TIMER_STATE_RUNNING:
    assert(k_timer_current == timer);
    k_timer_current = NULL;
    break;
  default:
    k_spinlock_release(&k_timer_queue_lock);
    return -EINVAL;
  }

  timer->state = K_TIMER_STATE_NONE;

  k_spinlock_release(&k_timer_queue_lock);

  return 0;
}

void
k_timer_tick(void)
{
  struct KListLink *link;
  struct KTimer *timer;

  k_spinlock_acquire(&k_timer_queue_lock);

  if (k_list_is_empty(&k_timer_queue)) {
    k_spinlock_release(&k_timer_queue_lock);
    return;
  }

  link  = k_timer_queue.next;
  timer = KLIST_CONTAINER(link, struct KTimer, link);

  assert(timer->remain != 0);

  timer->remain--;

  while (timer->remain == 0) {
    void (*callback)(void *) = timer->callback;
    void *callback_arg = timer->callback_arg;

    assert(timer->state == K_TIMER_STATE_ACTIVE);

    k_list_remove(link);

    k_timer_current = timer;
    timer->state = K_TIMER_STATE_RUNNING;

    k_spinlock_release(&k_timer_queue_lock);

    callback(callback_arg);

    k_spinlock_acquire(&k_timer_queue_lock);

    if (k_timer_current != NULL) {
      assert(k_timer_current == timer);
      assert(timer->state == K_TIMER_STATE_RUNNING);

      if (timer->period != 0) {
        timer->remain = timer->period;
        k_timer_enqueue(timer);
      } else {
        timer->state = K_TIMER_STATE_INACTIVE;
      }

      k_timer_current = NULL;
    }

    if (k_list_is_empty(&k_timer_queue))
      break;

    link  = k_timer_queue.next;
    timer = KLIST_CONTAINER(link, struct KTimer, link);
  }

  k_spinlock_release(&k_timer_queue_lock);
}

static void
k_timer_enqueue(struct KTimer *timer)
{
  struct KListLink *link;

  assert(k_spinlock_holding(&k_timer_queue_lock));

  assert(timer->remain != 0);

  KLIST_FOREACH(&k_timer_queue, link) {
    struct KTimer *other = KLIST_CONTAINER(link, struct KTimer, link);

    if (other->remain > timer->remain) {
      other->remain -= timer->remain;
      break;
    }

    timer->remain -= other->remain;
  }

  k_list_add_back(link, &timer->link);
  timer->state = K_TIMER_STATE_ACTIVE;
}

static void
k_timer_dequeue(struct KTimer *timer)
{
  struct KListLink *next;
  
  assert(k_spinlock_holding(&k_timer_queue_lock));

  next = timer->link.next;
  k_list_remove(&timer->link);

  assert(timer != NULL);
  assert(next != NULL);

  if (next != &k_timer_queue) {
    struct KTimer *next_timer;
    
    next_timer = KLIST_CONTAINER(next, struct KTimer, link);
    next_timer->remain += timer->remain;
  }
}
