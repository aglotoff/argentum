#include "core_private.h"

void
_k_timeout_create(struct KTimeoutEntry *entry)
{
  k_list_null(&entry->link);
  entry->remain = 0;
}

void
_k_timeout_destroy(struct KTimeoutEntry *entry)
{
  k_list_remove(&entry->link);
}

void
_k_timeout_queue_add(struct KListLink *queue,
                     struct KTimeoutEntry *entry,
                     k_tick_t delay)
{
  struct KListLink *next_link;

  k_assert(delay > 0);

  entry->remain = delay;

  K_LIST_FOREACH(queue, next_link) {
    struct KTimeoutEntry *next_entry;
    
    next_entry = K_CONTAINER_OF(next_link, struct KTimeoutEntry, link);

    if (next_entry->remain > entry->remain) {
      next_entry->remain -= entry->remain;
      break;
    }

    entry->remain -= next_entry->remain;
  }

  k_list_add_back(next_link, &entry->link);
}

void
_k_timeout_queue_remove(struct KListLink *queue, struct KTimeoutEntry *entry)
{
  struct KListLink *next_link = entry->link.next;

  k_assert(next_link != K_NULL);

  k_list_remove(&entry->link);

  if (next_link != queue) {
    struct KTimeoutEntry *next_entry;

    next_entry = K_CONTAINER_OF(next_link, struct KTimeoutEntry, link);
    next_entry->remain += entry->remain;
  }
}

void
_k_timeout_queue_adjust(struct KListLink *queue,
                        void (*callback)(struct KTimeoutEntry *),
                        k_tick_t delta_ticks)
{
  struct KListLink *link;
  struct KTimeoutEntry *entry;

  if (k_list_is_empty(queue) || (delta_ticks == 0))
    return;

  link = queue->next;
  entry = K_CONTAINER_OF(link, struct KTimeoutEntry, link);

  k_assert(entry->remain != 0);

  if (delta_ticks < 0) {
    entry->remain -= delta_ticks;
    return;
  }

  while (delta_ticks > 0) {
    k_tick_t ticks_to_subtract = delta_ticks;
    
    if (ticks_to_subtract < entry->remain) {
      entry->remain -= ticks_to_subtract;
      break;
    }

    if (ticks_to_subtract > entry->remain)
      ticks_to_subtract = entry->remain;

    entry->remain -= ticks_to_subtract;

    while (entry->remain == 0) {
      k_list_remove(link);

      callback(entry);

      if (k_list_is_empty(queue))
        return;

      link = queue->next;
      entry = K_CONTAINER_OF(link, struct KTimeoutEntry, link);
    }

    delta_ticks -= ticks_to_subtract;
  }
}
