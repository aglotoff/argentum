#include <arch/i386/mmu.h>
#include <arch/i386/regs.h>
#include <kernel/mm/memlayout.h>

#define RELOC(x) ((x) - VIRT_KERNEL_BASE)
#define MPBOOTPHYS(s) ((s) - mp_start + PHYS_MP_ENTRY)

.code16           
.globl mp_start
mp_start:
  cli            

  # Zero data segment registers DS, ES, and SS.
  xorw    %ax,%ax
  movw    %ax,%ds
  movw    %ax,%es
  movw    %ax,%ss

  # Switch from real to protected mode.  Use a bootstrap GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition.
  lgdt    MPBOOTPHYS(mp_gdtdesc)
  movl    %cr0, %eax
  orl     $CR0_PE, %eax
  movl    %eax, %cr0

  ljmpl    $(SEG_KERNEL_CODE), $(MPBOOTPHYS(start32))

.code32  
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KERNEL_DATA), %ax    # Our data segment selector
  movw    %ax, %ds                # -> DS: Data Segment
  movw    %ax, %es                # -> ES: Extra Segment
  movw    %ax, %ss                # -> SS: Stack Segment
  movw    $0, %ax                 # Zero segments not ready for use
  movw    %ax, %fs                # -> FS
  movw    %ax, %gs                # -> GS

  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4

  movl    $RELOC(entry_pgdir), %eax
  movl    %eax, %cr3

  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # Switch to the stack allocated by startothers()
  movl    mp_stack, %esp

  xorl    %ebp, %ebp

  # Call mpenter()
  movl    $arch_mp_entry, %eax
  call	  *%eax

  jmp      .

# Bootstrap GDT
.p2align 2
mp_gdt:
  SEG_DESC_NULL
  SEG_DESC_32(0x0, 0xffffffff, SEG_TYPE_CODE | SEG_TYPE_R, PL_KERNEL)
  SEG_DESC_32(0x0, 0xffffffff, SEG_TYPE_DATA | SEG_TYPE_W, PL_KERNEL)

mp_gdtdesc:
  .word   (mp_gdtdesc - mp_gdt - 1)
  .long   MPBOOTPHYS(mp_gdt)

.globl mp_end
mp_end:
  nop
