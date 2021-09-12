OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
  /* Link the boot code at virtual address 0 */
  . = 0x1000;

  .boot : {
    *(.boot*)
  }

  /* Link the rest of the kernel at high virtual addresses */
  . += 0x80000000;

  .text : AT(ADDR(.text) - 0x80000000) { 
    *(.text*)
    PROVIDE(_etext = .);
  }

  .rodata : AT(ADDR(.rodata) - 0x80000000) {
    *(.rodata*)
  }

  /* Include debugging information in kernel memory */
  .debug : AT(ADDR(.debug) - 0x80000000) {
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
  .= ALIGN(0x1000);

  .data : AT(ADDR(.data) - 0x80000000)  {
    *(.data*)
    PROVIDE(_edata = .);
  }

  .bss : AT(ADDR(.bss) - 0x80000000)  {
    *(.bss*)
    *(COMMON*)
    PROVIDE(_end = .);
  }

  /DISCARD/ : {
		*(.eh_frame .note.GNU-stack .comment .note)
	}
}
