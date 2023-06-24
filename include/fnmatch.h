#ifndef __INCLUDE_FNMATCH_H__
#define __INCLUDE_FNMATCH_H__

/**
 * @file
 * filename-matching types
 */

#define FNM_NOMATCH   1

/** <slash> in string only matches <slash> in pattern */
#define FNM_PATHNAME  (1 << 0)
/** Leading <period> in string must be exactly matched by <period> in pattern */
#define FNM_PERIOD    (1 << 1)
/** Disable backslash escaping */
#define FNM_NOESCAPE  (1 << 2)

int fnmatch(const char *, const char *, int);

#endif  // !__INCLUDE_FNMATCH__
