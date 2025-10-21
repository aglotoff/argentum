#ifndef __INCLUDE_KERNEL_CORE_LIST_H__
#define __INCLUDE_KERNEL_CORE_LIST_H__

/**
 * @file
 * 
 * Intrusive doubly linked list implementation.
 */

#include <kernel/core/assert.h>
#include <kernel/core/config.h>

/**
 * @brief Node structure for intrusive doubly linked lists.
 *
 * Each element in an intrusive list embeds one of these links to allow it
 * to participate in a list without additional allocation.
 */
struct KListLink {
  /** Pointer to the next node in the list. */
  struct KListLink *next;
  /** Pointer to the previous node in the list. */
  struct KListLink *prev;
};

/**
 * @brief Declare and initialize a list head at definition time.
 *
 * Creates a static list head named `name`, initialized to point to itself.
 * The resulting list is empty.
 *
 * @param name Name of the list head variable.
 */
#define K_LIST_DECLARE(name)     struct KListLink name = { &name, &name }

/**
 * @brief Static initializer for a list head.
 *
 * Provides an initializer for a `KListLink` that points to itself,
 * representing an empty list.
 *
 * @param head List head symbol.
 */
#define K_LIST_INITIALIZER(head) { &(head), &(head) }

/**
 * @brief Initialize a list head at runtime.
 *
 * Sets the list’s `next` and `prev` pointers to point to itself,
 * creating an empty circular list.
 *
 * @param head Pointer to the list head.
 */
static inline void
k_list_init(struct KListLink *head)
{
  head->prev = head->next = head;
}

/**
 * @brief Mark a list link as unlinked.
 *
 * Clears a link’s pointers by setting them to `K_NULL`.
 * This marks the link as not currently part of any list.
 *
 * @param link Pointer to the link to nullify.
 *
 * @note This is typically called after removal to avoid dangling pointers.
 */
static inline void
k_list_null(struct KListLink *link)
{
  link->prev = link->next = K_NULL;
}

/**
 * @brief Check whether a list is empty.
 *
 * Determines if the given list head has no elements.
 *
 * @param head Pointer to the list head.
 * @retval 1 The list is empty.
 * @retval 0 The list contains at least one element.
 */
static inline int
k_list_is_empty(struct KListLink *head)
{
  return head->next == head;
}

/**
 * @brief Check whether a link is null (unlinked).
 *
 * @param link Pointer to a list link.
 * @retval 1 The link is not part of any list.
 * @retval 0 The link is currently linked.
 */
static inline int
k_list_is_null(struct KListLink *link)
{
  return (link->next == K_NULL) && (link->prev == K_NULL);
}

/**
 * @brief Insert a link at the front of a list.
 *
 * Adds `link` immediately after the list head.
 *
 * @param head Pointer to the list head.
 * @param link Pointer to the new link to insert.
 *
 * @note The link must not already be part of another list.
 */
static inline void
k_list_add_front(struct KListLink *head, struct KListLink *link)
{
  k_assert(k_list_is_null(link));

  link->next = head->next;
  head->next->prev = link;
  head->next = link;
  link->prev = head;
}

/**
 * @brief Insert a link at the back of a list.
 *
 * Adds `link` immediately before the list head.
 *
 * @param head Pointer to the list head.
 * @param link Pointer to the new link to insert.
 *
 * @note The link must not already belong to another list.
 */
static inline void
k_list_add_back(struct KListLink *head, struct KListLink *link)
{
  k_assert(k_list_is_null(link));

  link->prev = head->prev;
  head->prev->next = link;
  head->prev = link;
  link->next = head;
}

/**
 * @brief Remove a link from a list.
 *
 * Unlinks `link` from whatever list it belongs to and sets its pointers to
 * `K_NULL`. It is safe to call this even if `link` is already unlinked.
 *
 * @param link Pointer to the link to remove.
 */
static inline void
k_list_remove(struct KListLink *link)
{
  if (link->prev != K_NULL)
    link->prev->next = link->next;
  if (link->next != K_NULL)
    link->next->prev = link->prev;

  link->prev = link->next = K_NULL;
}

/**
 * @brief Iterate over a list.
 *
 * Convenience macro for traversing a list in forward order.
 *
 * @param head Pointer to the list head.
 * @param lp   Iterator variable.
 *
 * @note The list must not be modified during iteration unless
 *       the current node has already been safely removed.
 */
#define K_LIST_FOREACH(head, lp) \
  for (lp = (head)->next; lp != (head); lp = lp->next)

#endif  // !__INCLUDE_KERNEL_CORE_LIST_H__
