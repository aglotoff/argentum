#ifndef __KERNEL_INCLUDE_KERNEL__LIST_H__
#define __KERNEL_INCLUDE_KERNEL__LIST_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/argentum/list.h
 * 
 * Intrusive doubly linked list implementation.
 */

#include <stddef.h>

struct ListLink {
  struct ListLink *next;
  struct ListLink *prev;
};

#define LIST_DECLARE(name)      struct ListLink name = { &name, &name }
#define LIST_INITIALIZER(head)  { &(head), &(head) }

static inline void
list_init(struct ListLink *head)
{
  head->prev = head->next = head;
}

static inline int
list_empty(struct ListLink *head)
{
  return head->next == head;
}

static inline void
list_add_front(struct ListLink *head, struct ListLink *link)
{
  link->next = head->next;
  head->next->prev = link;
  head->next = link;
  link->prev = head;
}

static inline void
list_add_back(struct ListLink *head, struct ListLink *link)
{
  link->prev = head->prev;
  head->prev->next = link;
  head->prev = link;
  link->next = head;
}

static inline void
list_remove(struct ListLink *link)
{
  if (link->prev != NULL)
    link->prev->next = link->next;

  if (link->next != NULL)
    link->next->prev = link->prev;

  link->prev = link->next = NULL;
}

#define LIST_CONTAINER(link, type, member) \
  ((type *) ((size_t) (link) - offsetof(type, member)))

#define LIST_FOREACH(head, lp) \
  for (lp = (head)->next; lp != (head); lp = lp->next)

#endif  // !__KERNEL_INCLUDE_KERNEL__LIST_H__
