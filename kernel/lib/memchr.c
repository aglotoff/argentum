#include <string.h>

/**
 * @brief Find byte in memory.
 *
 * Searches for the first occurence of c (converted to an unsigned char) in the
 * initial n bytes (each interpreted as unsigned char) pointed to by s.
 *
 * @param s Pointer to the block of memory where the search is performed.
 * @param c The value to be located.
 * @param n The number of bytes to examine.
 * 
 * @return A pointer to the first occurence of c, or a null pointer if the
 *         byte is not found.
 * 
 * @sa strchr, strcspn, strpbrk, strrchr, strspn, strstr, strtok
 */
void *
memchr(const void *s, int c, size_t n)
{
  const unsigned char *p = (const unsigned char *) s;
  unsigned char uc = (unsigned char) c;

  for ( ; n > 0; n--) {
    if (*p == uc)
      return (void *) p;
    p++;
  }

  return NULL;
}
