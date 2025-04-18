#ifndef __KERNEL_INCLUDE_KERNEL_CORE_LIST_H__
#define __KERNEL_INCLUDE_KERNEL_CORE_LIST_H__

/**
 * @file
 * 
 * Intrusive doubly linked list implementation.
 */

#include <kernel/core/assert.h>
#include <kernel/core/config.h>

struct KListLink {
  struct KListLink *next;
  struct KListLink *prev;
};

#define KLIST_DECLARE(name)     struct KListLink name = { &name, &name }
#define KLIST_INITIALIZER(head) { &(head), &(head) }

static inline void
k_list_init(struct KListLink *head)
{
  head->prev = head->next = head;
}

static inline void
k_list_null(struct KListLink *link)
{
  link->prev = link->next = NULL;
}

static inline int
k_list_is_empty(struct KListLink *head)
{
  return head->next == head;
}

static inline int
k_list_is_null(struct KListLink *link)
{
  return (link->next == NULL) && (link->prev == NULL);
}

static inline void
k_list_add_front(struct KListLink *head, struct KListLink *link)
{
  k_assert(k_list_is_null(link));

  link->next = head->next;
  head->next->prev = link;
  head->next = link;
  link->prev = head;
}

static inline void
k_list_add_back(struct KListLink *head, struct KListLink *link)
{
  k_assert(k_list_is_null(link));

  link->prev = head->prev;
  head->prev->next = link;
  head->prev = link;
  link->next = head;
}

static inline void
k_list_remove(struct KListLink *link)
{
  if (link->prev != NULL)
    link->prev->next = link->next;
  if (link->next != NULL)
    link->next->prev = link->prev;

  link->prev = link->next = NULL;
}

#define KLIST_CONTAINER(link, type, member) \
  ((type *) ((size_t) (link) - offsetof(type, member)))

#define KLIST_FOREACH(head, lp) \
  for (lp = (head)->next; lp != (head); lp = lp->next)

#endif  // !__KERNEL_INCLUDE_KERNEL_CORE_LIST_H__
