#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copy bytes in memory with overlapping areas.
 * 
 * @param s1 Pointer to the destination array where the data is to be copied.
 * @param s2 Pointer to the source of data to be copied.
 * @param n Number of bytes to copy.
 * 
 * @returns s1
 */
void *memmove(void *s1, const void *s2, size_t n);

/**
 * Set bytes in memory.
 *
 * @param s Pointer to the block of memory to fill.
 * @param c Value to be copied (interpreted as unsigned char).
 * @param n Number of bytes to be set.
 * 
 * @returns s.
 */
void *memset(void *s, int c, size_t n);

/**
 * Get length of fixed size string.
 *
 * @param s C string.
 * @return The length of the string.
 */
size_t strlen(const char *s);

#ifdef __cplusplus
};
#endif

#endif  // !INCLUDE_STRING_H
