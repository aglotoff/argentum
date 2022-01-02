#ifndef __INCLUDE_YVALS_H__
#define __INCLUDE_YVALS_H__

// Exact-width integer types
typedef signed char         __int8_t;
typedef unsigned char       __uint8_t;
typedef short               __int16_t;
typedef unsigned short      __uint16_t;
typedef int                 __int32_t;
typedef unsigned            __uint32_t;
typedef long long           __int64_t;
typedef unsigned long long  __uint64_t;
typedef long                __intptr_t;
typedef unsigned long       __uintptr_t;

// Data types
typedef long                __ptrdiff_t;
typedef unsigned long       __size_t;
typedef unsigned short      __wchar_t;

// The number of elements in jmp_buf (we must store R4-R11, IP, SP, and LR)
#define __NSETJMP   11

#endif  // !__INCLUDE_YVALS_H__
