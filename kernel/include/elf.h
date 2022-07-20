#ifndef __KERNEL_ELF_H__
#define __KERNEL_ELF_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#define EI_NIDENT        16

/**
 * ELF Header
 */
typedef struct {
  unsigned char ident[EI_NIDENT];   ///< ELF identification
  uint16_t      type;               ///< Object file type
  uint16_t      machine;            ///< Required architecture
  uint32_t      version;            ///< Object file version
  uint32_t      entry;              ///< Entry point virtual address
  uint32_t      phoff;              ///< Program header table's offset
  uint32_t      shoff;              ///< Section header table's offset
  uint32_t      flags;              ///< Processor-specific flags
  uint16_t      ehsize;             ///< ELF header size
  uint16_t      phentsize;          ///< Program header entry size
  uint16_t      phnum;              ///< The number of program header entries
  uint16_t      shentsize;          ///< Section header entry size
  uint16_t      shnum;              ///< The number of section header entries
  uint16_t      shstrndx;           ///< Section name string table index
} Elf32_Ehdr;

/**
 * Program Header
 */
typedef struct {
  uint32_t      type;               ///< Segment type
  uint32_t      offset;             ///< Segment offset
  uint32_t      vaddr;              ///< Segment virtual address
  uint32_t      paddr;              ///< Segment physical address
  uint32_t      filesz;             ///< Segment file image size
  uint32_t      memsz;              ///< Segment memory image size
  uint32_t      flags;              ///< Segment flags
  uint32_t      align;              ///< Segment alignment
} Elf32_Phdr;

#define PT_NULL     0               ///< Unused
#define PT_LOAD     1               ///< Loadable segment
#define PT_DYNAMIC  2               ///< Dynamic linking information
#define PT_INTERP   3               ///< Interpreter pathname
#define PT_NOTE     4               ///< Auxiliary information
#define PT_SHLIB    5               ///< Reserved
#define PT_PHDR     6               ///< Program header table
#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7fffffff

#endif  // !__KERNEL_ELF_H__
