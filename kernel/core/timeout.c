#include "core_private.h"

void
_k_timeout_create(struct KTimeoutEntry *timeout)
{
  k_list_null(&timeout->link);
  timeout->remain = 0;
}

void
_k_timeout_destroy(struct KTimeoutEntry *timeout)
{
  k_list_remove(&timeout->link);
}

void
_k_timeout_enqueue(struct KListLink *queue,
                   struct KTimeoutEntry *timeout,
                   k_tick_t delay)
{
  struct KListLink *next_link;

  k_assert(delay > 0);

  timeout->remain = delay;

  K_LIST_FOREACH(queue, next_link) {
    struct KTimeoutEntry *next;
    
    next = K_CONTAINER_OF(next_link, struct KTimeoutEntry, link);

    if (next->remain > timeout->remain) {
      next->remain -= timeout->remain;
      break;
    }

    timeout->remain -= next->remain;
  }

  k_list_add_back(next_link, &timeout->link);
}

void
_k_timeout_dequeue(struct KListLink *queue, struct KTimeoutEntry *timeout)
{
  struct KListLink *next_link = timeout->link.next;

  k_assert(next_link != K_NULL);

  k_list_remove(&timeout->link);

  if (next_link != queue) {
    struct KTimeoutEntry *next;

    next = K_CONTAINER_OF(next_link, struct KTimeoutEntry, link);
    next->remain += timeout->remain;
  }
}

void
_k_timeout_process_queue(struct KListLink *queue,
                         void (*callback)(struct KTimeoutEntry *))
{
  struct KListLink *link;
  struct KTimeoutEntry *timeout;

  if (k_list_is_empty(queue))
    return;

  link = queue->next;
  timeout = K_CONTAINER_OF(link, struct KTimeoutEntry, link);

  k_assert(timeout->remain != 0);

  timeout->remain--;

  while (timeout->remain == 0) {
    k_list_remove(link);

    callback(timeout);

    if (k_list_is_empty(queue))
      break;

    link = queue->next;
    timeout = K_CONTAINER_OF(link, struct KTimeoutEntry, link);
  }
}
