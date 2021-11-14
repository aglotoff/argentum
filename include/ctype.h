#ifndef __INCLUDE_CTYPE_H__
#define __INCLUDE_CTYPE_H__

/**
 * @file include/ctype.h
 * 
 * Character types.
 */

#ifdef __cplusplus
extern "C" {
#endif

int isalnum(int);
int isalpha(int);
int iscntrl(int);
int isdigit(int);
int isgraph(int);
int islower(int);
int isprint(int);
int ispunct(int);
int isspace(int);
int isupper(int);
int isxdigit(int);

int tolower(int);
int toupper(int);

#ifdef __cplusplus
};
#endif

enum {
  __CSPACE  = (1 << 1),     // Space character
  __CXSPACE = (1 << 2),     // Other whitespace characters
  __CDIGIT  = (1 << 3),     // Digits
  __CXDIGIT = (1 << 4),     // Hexadecimal digits
  __CUPPER  = (1 << 5),     // Uppercase characters
  __CLOWER  = (1 << 6),     // Lowercase characters
  __CXALPHA = (1 << 7),     // Oher alphabetical characters
  __CPUNCT  = (1 << 8),     // Punctuation
  __CXGRAPH = (1 << 9),     // Other visible characters
  __CCNTRL  = (1 << 10),    // Control characters

  __CALPHA  = __CUPPER | __CLOWER | __CXALPHA,  // All alphabetic characters
  __CALNUM  = __CALPHA | __CDIGIT,              // Alphanumeric characters
  __CGRAPH  = __CALNUM | __CPUNCT | __CXGRAPH,  // All visible characters
  __CPRINT  = __CGRAPH | __CSPACE,              // All printable characters
  __CWSPACE = __CSPACE | __CXSPACE,             // All whitespace characters
};

extern short __ctype[];

// Test whether the character c belongs to a class represented by the mask
#define __ctest(c, mask)  ((c) < 0 ? 0 : __ctype[(unsigned char)(c)] & (mask))

#define isalnum(c)  __ctest(c, __CALNUM)
#define isalpha(c)  __ctest(c, __CALPHA)
#define iscntrl(c)  __ctest(c, __CCNTRL)
#define isdigit(c)  __ctest(c, __CDIGIT)
#define isgraph(c)  __ctest(c, __CGRAPH)
#define islower(c)  __ctest(c, __CLOWER)
#define isprint(c)  __ctest(c, __CPRINT)
#define ispunct(c)  __ctest(c, __CPUNCT)
#define isspace(c)  __ctest(c, __CWSPACE)
#define isupper(c)  __ctest(c, __CUPPER)
#define isxdigit(c) __ctest(c, __CXDIGIT)

extern char __tolower[];
extern char __toupper[];

#define tolower(c)  ((c) < 0 ? (c) : __tolower[(unsigned char)(c)])
#define toupper(c)  ((c) < 0 ? (c) : __toupper[(unsigned char)(c)])

#endif  // !__INCLUDE_CTYPE_H__
