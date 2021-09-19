OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
  /* Link the entry code at virtual address 0 */
  . = 0x0;

  .text : { 
    *(.text*)
    PROVIDE(_etext = .);
  }

  .rodata : {
    *(.rodata*)
  }

  /* Adjust the address of the data section to the next page boundary */
  .= ALIGN(0x1000);

  .data : {
    *(.data*)
    PROVIDE(_edata = .);
  }

  .bss : {
    *(.bss*)
    *(COMMON*)
    PROVIDE(_end = .);
  }

  /DISCARD/ : {
		*(.eh_frame .note.GNU-stack .comment .note)
	}
}
