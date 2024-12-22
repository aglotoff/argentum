
#define ALIGN    1<<0            
#define MEMINFO   1<<1
#define FLAGS     (ALIGN | MEMINFO)
#define MAGIC     0x1BADB002
#define CHECKSUM  -(MAGIC + FLAGS)

.section .entry, "a"
.align 4

.global _multiboot
_multiboot:
.long MAGIC
.long FLAGS
.long CHECKSUM

.global _start
.type _start, @function
_start:
	mov $stack_top, %esp

	call arch_init

	cli
1:	hlt
	jmp 1b

.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:
