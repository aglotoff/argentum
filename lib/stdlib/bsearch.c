#include <stdlib.h>

/**
 * Search a sorted array of nmemb objects.
 *
 * @param  key    Pointer to an object that serves as a search key.
 * @param  base   Pointer to the initial element of the array.
 * @param  nmemb  The number of elements in the array.
 * @param  size   The size of each element in the array in bytes.
 * @param  compar The comparison function.
 * @return A pointer to a matching element in the array or NULL if no match is
 *         found.
 */
void
*bsearch(const void *key, const void *base, size_t nmemb, size_t size,
         int (*compar)(const void *, const void *))
{
  long lo = 0, hi = nmemb - 1;

  while (lo <= hi) {
    long mid = (lo + hi) / 2;
    const char *elem = (const char *) base + mid * size;
    int res = compar(key, elem);

    if (res < 0)
      hi = mid - 1;
    else if (res > 0)
      lo = mid + 1;
    else
      return (void *) elem;
  }

  return NULL;
}
