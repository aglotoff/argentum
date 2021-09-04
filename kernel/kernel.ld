OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
  /* Link the boot code at virtual address 0 */
  . = 0;

  .boot : {
    *(.boot*)
  }

  /* Link the rest of the kernel at high virtual addresses */
  . += 0x80000000;

  .text : AT(ADDR(.text) - 0x80000000) { 
    *(.text*)
    PROVIDE(_etext = .);
  }

  .rodata : AT(ADDR(.rodata) - 0x80000000)  {
    *(.rodata*)
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
}
