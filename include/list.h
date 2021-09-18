#ifndef INCLUDE_LIST_H
#define INCLUDE_LIST_H

/**
 * @file include/list.h
 * 
 * Intrusive doubly linked list implementation.
 */

#include <stddef.h>

struct ListLink {
  struct ListLink *next;
  struct ListLink *prev;
};

#define LIST_DECLARE(name)    struct ListLink name = { &name, &name } 

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
  // cprintf("  list_add_front(%p, %p)\n", head, link);
  // cprintf("    head->next = %p\n", head->next);

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
  // cprintf("  list_remove(%p)\n", link);

  if (link->prev != NULL) {
    link->prev->next = link->next;
    link->prev = NULL;
  }
  if (link->next != NULL) {
    link->next->prev = link->prev;
    link->next = NULL;
  }
}

#define LIST_CONTAINER(link, type, member) \
  ((type *) ((size_t) (link) - offsetof(type, member)))

#define LIST_FOREACH(head, lp) \
  for (lp = (head)->next; lp != (head); lp = lp->next)

#endif  // !INCLUDE_LIST_H
