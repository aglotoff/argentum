ENTRY (_start)

SECTIONS
{
  . = 0;

  .text : { 
    *(.text*)
  }

  .rodata : {
    *(.rodata*)
  }

  .= ALIGN(0x1000);

  .data : {
    *(.data*)
  }

  .bss : {
    *(.bss*)
    *(COMMON*)
  }
}
