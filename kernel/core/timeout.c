#include <kernel/core/config.h>

#include "core_private.h"

/*
 * Timeout queue management functions used internally by application timers and
 * sleeping tasks' timeouts.
 *
 * Each `KTimeoutEntry` stores the number of ticks (`remain`) until its own
 * expiration relative to the preceding entry. The head of the queue therefore
 * represents the next timeout to expire. When an entry expires, it is removed
 * and its callback is invoked and the next entry becomes the new head. This
 * structure minimizes update cost during tick advancement and supports
 * efficient insertion and removal while maintaining strict timeout ordering.
 */

void
_k_timeout_entry_create(struct KTimeoutEntry *entry)
{
  k_list_null(&entry->link);
  entry->remain = 0;
}

void
_k_timeout_entry_destroy(struct KTimeoutEntry *entry)
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
      // Found insertion point: split the successor's delta so that
      // the new entry fits precisely before it.
      next_entry->remain -= entry->remain;
      break;
    }

    // Skip over this entry
    entry->remain -= next_entry->remain;
  }

  // Insert before `next_link` (either the first larger entry or the queue end).
  k_list_add_back(next_link, &entry->link);
}

void
_k_timeout_queue_remove(struct KListLink *queue, struct KTimeoutEntry *entry)
{
  struct KListLink *next_link;
  
  k_assert(!k_list_is_null(&entry->link));

  next_link = entry->link.next;
  k_list_remove(&entry->link);

  // Merge the removed entry's relative timeout into the successor,
  // preserving cumulative timing of the delta-encoded list.
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
    // Negative delta: push the head entry further into the "future"
    entry->remain += -delta_ticks;
    return;
  }

  // Positive delta: simulate the passage of time.

  // FIXME: Large positive deltas may cause a cascade of expired timers!

  while (delta_ticks > 0) {
    k_tick_t ticks_to_subtract = delta_ticks;
    
    if (ticks_to_subtract < entry->remain) {
      // The head entry does not fully expire within this delta
      entry->remain -= ticks_to_subtract;
      break;
    }

    if (ticks_to_subtract > entry->remain) {
      // Clamp to the remaining time of the current head entry
      ticks_to_subtract = entry->remain;
    }

    entry->remain -= ticks_to_subtract;

    // For each expired entry, remove it and invoke the callback
    while (entry->remain == 0) {
      k_list_remove(link);

      callback(entry);

      if (k_list_is_empty(queue))
        return;

      link = queue->next;
      entry = K_CONTAINER_OF(link, struct KTimeoutEntry, link);
    }

    // Reduce delta by whatever amount was actually consumed and continue
    delta_ticks -= ticks_to_subtract;
  }
}
