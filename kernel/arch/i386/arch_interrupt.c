#include <kernel/interrupt.h>

#include <arch/trap.h>
#include <arch/i386/mmu.h>
#include <arch/i386/lapic.h>
#include <arch/i386/i8253.h>
#include <arch/i386/i8259.h>
#include <arch/i386/ioapic.h>

void
arch_interrupt_ipi(void)
{
  // TODO
}

int
arch_interrupt_id(struct TrapFrame *tf)
{
  return tf->trapno - T_IRQ0;
}

void
arch_interrupt_enable(int irq, int cpu)
{
  ioapic_enable(irq, cpu);
}

void
arch_interrupt_mask(int irq)
{
  ioapic_mask(irq);
}

void
arch_interrupt_unmask(int irq)
{
  ioapic_unmask(irq);
}

void
arch_interrupt_eoi(int)
{
  lapic_eoi();
}

struct IDTGate idt[256];

struct PseudoDesc idtr = {
  .limit = sizeof idt - 1,
  .base  = (uintptr_t) idt,
};

extern void trap_de(void);
extern void trap_db(void);
extern void trap_bp(void);
extern void trap_of(void);
extern void trap_br(void);
extern void trap_ud(void);
extern void trap_nm(void);
extern void trap_df(void);
extern void trap_ts(void);
extern void trap_np(void);
extern void trap_ss(void);
extern void trap_gp(void);
extern void trap_pf(void);
extern void trap_mf(void);
extern void trap_ac(void);
extern void trap_mc(void);
extern void trap_xf(void);

extern void trap_irq0(void);
extern void trap_irq1(void);
extern void trap_irq2(void);
extern void trap_irq3(void);
extern void trap_irq4(void);
extern void trap_irq5(void);
extern void trap_irq6(void);
extern void trap_irq7(void);
extern void trap_irq8(void);
extern void trap_irq9(void);
extern void trap_irq10(void);
extern void trap_irq11(void);
extern void trap_irq12(void);
extern void trap_irq13(void);
extern void trap_irq14(void);
extern void trap_irq15(void);
extern void trap_irq16(void);
extern void trap_irq17(void);
extern void trap_irq18(void);
extern void trap_irq19(void);
extern void trap_irq20(void);
extern void trap_irq21(void);
extern void trap_irq22(void);
extern void trap_irq23(void);
extern void trap_irq24(void);
extern void trap_irq25(void);
extern void trap_irq26(void);
extern void trap_irq27(void);
extern void trap_irq28(void);
extern void trap_irq29(void);
extern void trap_irq30(void);
extern void trap_irq31(void);

extern void trap_syscall(void);

void
arch_interrupt_init(void)
{
  idt[T_DE] = IDT_GATE(SEG_TYPE_IG32, trap_de, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_DB] = IDT_GATE(SEG_TYPE_IG32, trap_db, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_BP] = IDT_GATE(SEG_TYPE_IG32, trap_bp, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_OF] = IDT_GATE(SEG_TYPE_IG32, trap_of, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_BR] = IDT_GATE(SEG_TYPE_IG32, trap_br, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_UD] = IDT_GATE(SEG_TYPE_IG32, trap_ud, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_NM] = IDT_GATE(SEG_TYPE_IG32, trap_nm, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_DF] = IDT_GATE(SEG_TYPE_IG32, trap_df, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_TS] = IDT_GATE(SEG_TYPE_IG32, trap_ts, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_NP] = IDT_GATE(SEG_TYPE_IG32, trap_np, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_SS] = IDT_GATE(SEG_TYPE_IG32, trap_ss, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_GP] = IDT_GATE(SEG_TYPE_IG32, trap_gp, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_PF] = IDT_GATE(SEG_TYPE_IG32, trap_pf, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_MF] = IDT_GATE(SEG_TYPE_IG32, trap_mf, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_AC] = IDT_GATE(SEG_TYPE_IG32, trap_ac, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_MC] = IDT_GATE(SEG_TYPE_IG32, trap_mc, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_XF] = IDT_GATE(SEG_TYPE_IG32, trap_xf, SEG_KERNEL_CODE, PL_KERNEL);

  idt[T_IRQ0 + 0] = IDT_GATE(SEG_TYPE_IG32, trap_irq0, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 1] = IDT_GATE(SEG_TYPE_IG32, trap_irq1, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 2] = IDT_GATE(SEG_TYPE_IG32, trap_irq2, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 3] = IDT_GATE(SEG_TYPE_IG32, trap_irq3, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 4] = IDT_GATE(SEG_TYPE_IG32, trap_irq4, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 5] = IDT_GATE(SEG_TYPE_IG32, trap_irq5, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 6] = IDT_GATE(SEG_TYPE_IG32, trap_irq6, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 7] = IDT_GATE(SEG_TYPE_IG32, trap_irq7, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 8] = IDT_GATE(SEG_TYPE_IG32, trap_irq8, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 9] = IDT_GATE(SEG_TYPE_IG32, trap_irq9, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 10] = IDT_GATE(SEG_TYPE_IG32, trap_irq10, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 11] = IDT_GATE(SEG_TYPE_IG32, trap_irq11, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 12] = IDT_GATE(SEG_TYPE_IG32, trap_irq12, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 13] = IDT_GATE(SEG_TYPE_IG32, trap_irq13, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 14] = IDT_GATE(SEG_TYPE_IG32, trap_irq14, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 15] = IDT_GATE(SEG_TYPE_IG32, trap_irq15, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 16] = IDT_GATE(SEG_TYPE_IG32, trap_irq16, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 17] = IDT_GATE(SEG_TYPE_IG32, trap_irq17, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 18] = IDT_GATE(SEG_TYPE_IG32, trap_irq18, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 19] = IDT_GATE(SEG_TYPE_IG32, trap_irq19, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 20] = IDT_GATE(SEG_TYPE_IG32, trap_irq20, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 21] = IDT_GATE(SEG_TYPE_IG32, trap_irq21, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 22] = IDT_GATE(SEG_TYPE_IG32, trap_irq22, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 23] = IDT_GATE(SEG_TYPE_IG32, trap_irq23, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 24] = IDT_GATE(SEG_TYPE_IG32, trap_irq24, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 25] = IDT_GATE(SEG_TYPE_IG32, trap_irq25, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 26] = IDT_GATE(SEG_TYPE_IG32, trap_irq26, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 27] = IDT_GATE(SEG_TYPE_IG32, trap_irq27, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 28] = IDT_GATE(SEG_TYPE_IG32, trap_irq28, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 29] = IDT_GATE(SEG_TYPE_IG32, trap_irq29, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 30] = IDT_GATE(SEG_TYPE_IG32, trap_irq30, SEG_KERNEL_CODE, PL_KERNEL);
  idt[T_IRQ0 + 31] = IDT_GATE(SEG_TYPE_IG32, trap_irq31, SEG_KERNEL_CODE, PL_KERNEL);

  idt[T_SYSCALL] = IDT_GATE(SEG_TYPE_TG32, trap_syscall, SEG_KERNEL_CODE, PL_USER);

  i8259_mask_all();

  arch_interrupt_init_percpu();
}

void
arch_interrupt_init_percpu(void)
{
  lidt(&idtr);

  lapic_init();
  ioapic_init();
}
