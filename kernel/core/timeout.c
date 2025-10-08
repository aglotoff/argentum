#include "core_private.h"

void
_k_timeout_create(struct KTimeout *timeout)
{
  k_list_null(&timeout->link);
  timeout->remain = 0;
}

void
_k_timeout_destroy(struct KTimeout *timeout)
{
  k_list_remove(&timeout->link);
}

void
_k_timeout_enqueue(struct KListLink *queue,
                   struct KTimeout *timeout,
                   k_tick_t delay)
{
  struct KListLink *next_link;

  k_assert(delay > 0);

  timeout->remain = delay;

  K_LIST_FOREACH(queue, next_link) {
    struct KTimeout *next = K_LIST_CONTAINER(next_link, struct KTimeout, link);

    if (next->remain > timeout->remain) {
      next->remain -= timeout->remain;
      break;
    }

    timeout->remain -= next->remain;
  }

  k_list_add_back(next_link, &timeout->link);
}

void
_k_timeout_dequeue(struct KListLink *queue, struct KTimeout *timeout)
{
  struct KListLink *next_link = timeout->link.next;

  k_assert(next_link != NULL);

  k_list_remove(&timeout->link);

  if (next_link != queue) {
    struct KTimeout *next = K_LIST_CONTAINER(next_link, struct KTimeout, link);
    next->remain += timeout->remain;
  }
}

void
_k_timeout_process_queue(struct KListLink *queue,
                         void (*callback)(struct KTimeout *))
{
  struct KListLink *link;
  struct KTimeout *timeout;

  if (k_list_is_empty(queue))
    return;

  link = queue->next;
  timeout = K_LIST_CONTAINER(link, struct KTimeout, link);

  k_assert(timeout->remain != 0);

  timeout->remain--;

  while (timeout->remain == 0) {
    k_list_remove(link);

    callback(timeout);

    if (k_list_is_empty(queue))
      break;

    link = queue->next;
    timeout = K_LIST_CONTAINER(link, struct KTimeout, link);
  }
}
