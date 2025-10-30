
#include <kernel/mm/memlayout.h>
#include <kernel/multiboot.h>

#include <arch/i386/regs.h>

#define MULTIBOOT_HEADER_FLAGS   	(MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)

.section .entry, "a"
.align 4

.global multiboot_header
multiboot_header:
.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_HEADER_FLAGS
.long -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

.global _start
_start:
	movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4

  movl    $(KVA2PA(entry_pgdir)), %eax
  movl    %eax, %cr3

  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

	mov     $kstack_top, %esp

	movl    $higher_half, %eax
	jmp     *%eax

.text

higher_half:
  xorl    %ebp, %ebp

	call    arch_init

	cli

1:	hlt
	jmp 1b

.bss

  .globl    kstack, kstack_top
  .p2align  12
kstack:
  .space    (KSTACK_SIZE * 4)
kstack_top:
