#include <stdlib.h>
#include <stdio.h>

//
// Quick sort with the following algorithmic improvements:
//  * Cutoff to insertion sort for tiny subarrays
//  * Median-of-three partitioning
//  * 3-way partitioning
//

static void shuffle(char *, size_t, size_t);
static void qsort_internal(char *, char *, size_t,
                           int (*)(const void *, const void *));

/**
 * Sort an array of objects.
 * 
 * @param base   Pointer to the initial array element.
 * @param nmemb  The number of objects in the array.
 * @param size   The size of each object.
 * @param compar The comparison function which is called with two arguments
 *               that point to the objects being compared. The function shall
 *               return an integer less than, equal to, or greater than zero if
 *               the first argument is considered to be respectively less than,
 *               equal to, or greater than the second.
 */
void
qsort(void *base, size_t nmemb, size_t size,
      int (*compar)(const void *, const void *))
{
  char *a = (char *) base;

  shuffle(a, nmemb, size);
  qsort_internal(a, &a[nmemb * size], size, compar);
}

static void
swap(char *a, char *b, size_t size)
{
  char t;

  for ( ; size > 0; size--) {
    t = *a;
    *a++ = *b;
    *b++ = t;
  }
}

static void
shuffle(char *a, size_t nmemb, size_t size)
{
  size_t i, r;
  unsigned seed = 0;

  for (i = 0; i < nmemb; i++) {
    r = i + rand_r(&seed) % (nmemb - i);
    swap(&a[i * size], &a[r * size], size);
  }
}

#define INSERTION_SORT_CUTOFF 10

static void
insertion_sort(char *begin, char *end, size_t size,
               int (*compar)(const void *, const void *))
{
  char *p, *q;

  for (p = begin; p < end; p += size)
    for (q = p + size; q > begin && compar(q - size, q) > 0; q -= size)
      swap(q - size, q, size);
}

static char *
med3(char *begin, char *end, size_t size,
     int (*compar)(const void *, const void *))
{
  char *a, *b, *c;

  a = begin;
  b = begin + (((end - begin - size) / size) / 2) * size;
  c = end - size;
  
  if (compar(a, b) < 0) {
    if (compar(b, c) < 0) {
      return b;
    } else if (compar(a, c) < 0) {
      return c;
    } else {
      return a;
    }
  } else {
    if (compar(a, c) < 0) {
      return a;
    } else if (compar(b, c)) {
      return c;
    } else {
      return b;
    }
  }
}

static char *
partition(char *begin, char *end, size_t size,
          int (*compar)(const void *, const void *), char **pgt)
{
  char *lt, *gt, *p;

  // Use the median of three for the pivot value
  // TODO: for huge arrays, could use Tukey's median of medians
  p = med3(begin, end, size, compar);
  if (p != begin)
    swap(p, begin, size);

  // TODO: implement fast 3-way partitioning (J.Bentley and D.McIlroy).
  for (lt = p = begin, gt = end; p < gt; ) {
    int cmp = compar(p, lt);

    if (cmp < 0) {
      swap(p, lt, size);
      lt += size;
      p  += size;
    } else if (cmp > 0) {
      gt -= size;
      swap(p, gt, size);
    } else {
      p += size;
    }
  }

  // Now [begin, lt) < v = [lt, gt) < [gt, end)

  *pgt = gt;
  return lt;
}

static void
qsort_internal(char *begin, char *end, size_t size,
               int (*compar)(const void *, const void *))
{
  char *lt, *gt;

  if (begin + INSERTION_SORT_CUTOFF * size >= end) {
    insertion_sort(begin, end, size, compar);
    return;
  }

  lt = partition(begin, end, size, compar, &gt);

  qsort_internal(begin, lt, size, compar);
  qsort_internal(gt, end, size, compar);
}
