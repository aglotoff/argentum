#ifndef INCLUDE_ELF_H
#define INCLUDE_ELF_H

#include <stdint.h>

#define EI_NIDENT        16

typedef struct {
  unsigned char ident[EI_NIDENT];
  uint16_t      type;
  uint16_t      machine;
  uint32_t      version;
  uint32_t      entry;
  uint32_t      phoff;
  uint32_t      shoff;
  uint32_t      flags;
  uint16_t      ehsize;
  uint16_t      phentsize;
  uint16_t      phnum;
  uint16_t      shentsize;
  uint16_t      shnum;
  uint16_t      shstrndx;
} Elf32_Ehdr;

typedef struct {
  uint32_t      type;
  uint32_t      offset;
  uint32_t      vaddr;
  uint32_t      paddr;
  uint32_t      filesz;
  uint32_t      memsz;
  uint32_t      flags;
  uint32_t      align;
} Elf32_Phdr;

#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6
#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7fffffff

#endif  // !INCLUDE_ELF_H
