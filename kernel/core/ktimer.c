#include <kernel/assert.h>
#include <errno.h>

#include <kernel/cprintf.h>
#include <kernel/ktimer.h>
#include <kernel/spin.h>

static void ktimer_enqueue(struct KTimer *);
static void ktimer_dequeue(struct KTimer *);

static LIST_DECLARE(ktimer_queue);
static struct SpinLock ktimer_queue_lock = SPIN_INITIALIZER("ktimer");

int
ktimer_create(struct KTimer *timer,
              void (*callback)(void *), void *callback_arg,
              unsigned long delay, unsigned long period,
              int autostart)
{
  if (timer == NULL)
    panic("timer is NULL");

  list_init(&timer->link);

  timer->callback     = callback;
  timer->callback_arg = callback_arg;
  timer->remain       = delay;
  timer->period       = period;

  spin_lock(&ktimer_queue_lock);

  if (autostart) {
    timer->state = KTIMER_STATE_ACTIVE;
    ktimer_enqueue(timer);
  } else {
    timer->state = KTIMER_STATE_INACTIVE;
  }

  spin_unlock(&ktimer_queue_lock);

  return 0;
}

int
ktimer_start(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  spin_lock(&ktimer_queue_lock);

  if (timer->state != KTIMER_STATE_INACTIVE) {
    spin_unlock(&ktimer_queue_lock);
    return -EINVAL;
  }

  timer->state = KTIMER_STATE_ACTIVE;
  ktimer_enqueue(timer);

  spin_unlock(&ktimer_queue_lock);

  return 0;
}

int
ktimer_stop(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  spin_lock(&ktimer_queue_lock);

  if (timer->state != KTIMER_STATE_ACTIVE) {
    spin_unlock(&ktimer_queue_lock);
    return -EINVAL;
  }

  ktimer_dequeue(timer);
  timer->state = KTIMER_STATE_INACTIVE;

  spin_unlock(&ktimer_queue_lock);

  return 0;
}

int
ktimer_destroy(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  spin_lock(&ktimer_queue_lock);

  // Already destroyed
  if (timer->state == KTIMER_STATE_NONE) {
    spin_unlock(&ktimer_queue_lock);
    return -EINVAL;
  }

  if (timer->state == KTIMER_STATE_ACTIVE)
    ktimer_dequeue(timer);

  timer->state = KTIMER_STATE_NONE;

  spin_unlock(&ktimer_queue_lock);

  return 0;
}

void
ktimer_tick(void)
{
  struct ListLink *link;
  struct KTimer *timer;

  spin_lock(&ktimer_queue_lock);

  if (list_empty(&ktimer_queue)) {
    spin_unlock(&ktimer_queue_lock);
    return;
  }

  link  = ktimer_queue.next;
  timer = LIST_CONTAINER(link, struct KTimer, link);

  assert(timer->state == KTIMER_STATE_ACTIVE);

  timer->remain--;

  while (timer->remain == 0) {
    list_remove(link);

    spin_unlock(&ktimer_queue_lock);

    // TODO: race condition! timer can be destroyed and its memory freed!!!

    timer->callback(timer->callback_arg);

    spin_lock(&ktimer_queue_lock);

    if (timer->state == KTIMER_STATE_ACTIVE) {
      if (timer->period != 0) {
        timer->remain = timer->period;
        ktimer_enqueue(timer);
      } else {
        timer->state = KTIMER_STATE_INACTIVE;
      }
    }

    if (list_empty(&ktimer_queue))
      break;

    link  = ktimer_queue.next;
    timer = LIST_CONTAINER(link, struct KTimer, link);

    assert(timer->state == KTIMER_STATE_ACTIVE);
  }

  spin_unlock(&ktimer_queue_lock);
}

static void
ktimer_enqueue(struct KTimer *timer)
{
  struct ListLink *link;

  assert(spin_holding(&ktimer_queue_lock));

  LIST_FOREACH(&ktimer_queue, link) {
    struct KTimer *other = LIST_CONTAINER(link, struct KTimer, link);

    if (other->remain > timer->remain) {
      other->remain -= timer->remain;
      break;
    }

    timer->remain -= other->remain;
  }

  list_add_back(link, &timer->link);
}

static void
ktimer_dequeue(struct KTimer *timer)
{
  struct ListLink *next;
  
  assert(spin_holding(&ktimer_queue_lock));

  next = timer->link.next;
  list_remove(&timer->link);

  if (next != &ktimer_queue) {
    struct KTimer *next_timer;
    
    next_timer = LIST_CONTAINER(next, struct KTimer, link);
    next_timer->remain += timer->remain;
  }
}
