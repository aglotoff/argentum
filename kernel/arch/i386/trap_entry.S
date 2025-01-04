#include <arch/trap.h>
#include <arch/i386/mmu.h>

.text

#define TRAP_HANDLER(name, n) \
	.globl name;                \
	.type name, @function;      \
	name:                       \
	pushl $(n);                 \
	jmp trap_common

#define TRAP_HANDLER_NO_ERROR(name, n) 	\
	.globl name;                					\
	.type name, @function;      					\
	name:                       					\
	pushl $0;															\
	pushl $(n);                 					\
	jmp trap_common

.globl trap_common
trap_common:
	pushl		%ds
	pushl		%es
	pushl		%fs
	pushl		%gs

	pusha

	movw    $SEG_KERNEL_DATA, %ax
	movw    %ax, %ds
	movw    %ax, %es

	pushl %esp
  call 	trap
	addl  $4, %esp

	popa

	popl  %gs
	popl  %fs
	popl  %es
	popl  %ds

	addl  $8, %esp

	iret

TRAP_HANDLER_NO_ERROR(trap_de, T_DE)
TRAP_HANDLER_NO_ERROR(trap_db, T_DB)
TRAP_HANDLER_NO_ERROR(trap_bp, T_BP)
TRAP_HANDLER_NO_ERROR(trap_of, T_OF)
TRAP_HANDLER_NO_ERROR(trap_br, T_BR)
TRAP_HANDLER_NO_ERROR(trap_ud, T_UD)
TRAP_HANDLER_NO_ERROR(trap_nm, T_NM)
TRAP_HANDLER(trap_df, T_DF)
TRAP_HANDLER(trap_ts, T_TS)
TRAP_HANDLER(trap_np, T_NP)
TRAP_HANDLER(trap_ss, T_SS)
TRAP_HANDLER(trap_gp, T_GP)
TRAP_HANDLER(trap_pf, T_PF)
TRAP_HANDLER_NO_ERROR(trap_mf, T_MF)
TRAP_HANDLER(trap_ac, T_AC)
TRAP_HANDLER_NO_ERROR(trap_mc, T_MC)
TRAP_HANDLER_NO_ERROR(trap_xf, T_XF)

TRAP_HANDLER_NO_ERROR(trap_irq0, T_IRQ0 + 0)
TRAP_HANDLER_NO_ERROR(trap_irq1, T_IRQ0 + 1)
TRAP_HANDLER_NO_ERROR(trap_irq2, T_IRQ0 + 2)
TRAP_HANDLER_NO_ERROR(trap_irq3, T_IRQ0 + 3)
TRAP_HANDLER_NO_ERROR(trap_irq4, T_IRQ0 + 4)
TRAP_HANDLER_NO_ERROR(trap_irq5, T_IRQ0 + 5)
TRAP_HANDLER_NO_ERROR(trap_irq6, T_IRQ0 + 6)
TRAP_HANDLER_NO_ERROR(trap_irq7, T_IRQ0 + 7)
TRAP_HANDLER_NO_ERROR(trap_irq8, T_IRQ0 + 8)
TRAP_HANDLER_NO_ERROR(trap_irq9, T_IRQ0 + 9)
TRAP_HANDLER_NO_ERROR(trap_irq10, T_IRQ0 + 10)
TRAP_HANDLER_NO_ERROR(trap_irq11, T_IRQ0 + 11)
TRAP_HANDLER_NO_ERROR(trap_irq12, T_IRQ0 + 12)
TRAP_HANDLER_NO_ERROR(trap_irq13, T_IRQ0 + 13)
TRAP_HANDLER_NO_ERROR(trap_irq14, T_IRQ0 + 14)
TRAP_HANDLER_NO_ERROR(trap_irq15, T_IRQ0 + 15)
