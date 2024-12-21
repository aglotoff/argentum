OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
  /* Link the entry code at virtual address 0x10000 */
  . = 0x100000;

  .entry : {
    *(.entry*)
  }

  /* Link the rest of the kernel at high virtual addresses */
  /* . += 0x80000000; */

  .text : AT(ADDR(.text)) { 
    *(.text*)
    PROVIDE(_etext = .);
  }

  .rodata : AT(ADDR(.rodata)) {
    *(.rodata*)
  }

  .mach (READONLY) : AT(ADDR(.mach)) {
    PROVIDE(__mach_begin__ = .);
    *(.mach*)
    PROVIDE(__mach_end__ = .);

    /* Force the linker to allocate space for this section */
    BYTE(0)
  }

  /* Include debugging information in kernel memory */
  .debug : AT(ADDR(.debug)) {
    PROVIDE(__debug_info_begin__ = .);
    *(.debug_info*)
    PROVIDE(__debug_info_end__ = .);
  
    . = ALIGN(8);

    PROVIDE(__debug_aranges_begin__ = .);
    *(.debug_aranges*)
    PROVIDE(__debug_aranges_end__ = .);

    PROVIDE(__debug_line_begin__ = .);
    *(.debug_line*)
    PROVIDE(__debug_line_end__ = .);

    PROVIDE(__debug_abbrev_begin__ = .);
    *(.debug_abbrev)
    PROVIDE(__debug_abbrev_end__ = .);

    PROVIDE(__debug_str_begin__ = .);
    *(.debug_str*)
    PROVIDE(__debug_str_end__ = .);

    /* Force the linker to allocate space for this section */
    BYTE(0)
  }

  /* Adjust the address of the data section to the next page boundary */
  . = ALIGN(0x1000);

  .data : AT(ADDR(.data))  {
    *(.data*)
    PROVIDE(_edata = .);
  }

  .bss : AT(ADDR(.bss))  {
    *(.bss*)
    *(COMMON*)
    PROVIDE(_end = .);
  }

  /DISCARD/ : {
		*(.eh_frame .note.GNU-stack .comment .note)
	}
}
