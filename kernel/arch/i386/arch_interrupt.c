#include <kernel/interrupt.h>
#include <kernel/time.h>

#include <arch/trap.h>
#include <arch/i386/mmu.h>
#include <arch/i386/io.h>

#define PIC_MASTER_COMMAND  0x20
#define PIC_MASTER_DATA     0x21
#define PIC_SLAVE_COMMAND   0xA0
#define PIC_SLAVE_DATA      0xA1

#define PIC_EOI             0x20

#define IRQ_SLAVE	2

#define ICW1_ICW4	      (1 << 0)		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE	    (1 << 1)		/* Single (cascade) mode */
#define ICW1_INTERVAL4	(1 << 2)		/* Call address interval 4 (8) */
#define ICW1_LEVEL	    (1 << 3)		/* Level triggered (edge) mode */
#define ICW1_INIT	      (1 << 4)		/* Initialization - required! */

#define ICW4_8086	      (1 << 0)		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	      (1 << 1)		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	(1 << 3)		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	(3 << 3)		/* Buffered mode/master */
#define ICW4_SFNM	      (1 << 4)		/* Special fully nested (not) */

static uint16_t i8259_mask = 0xFFFF & ~(1 << IRQ_SLAVE);;

static void
i8259_set_mask(uint16_t mask)
{
	i8259_mask = mask;

	outb(PIC_MASTER_DATA, mask & 0xFF);
	outb(PIC_SLAVE_DATA, (mask >> 8) & 0xFF);
}

static void
i8259_init(void)
{
	outb(PIC_MASTER_DATA, 0xFF);                        // save masks
	outb(PIC_SLAVE_DATA, 0xFF);
	
  // ICW1: start initialization
	outb(PIC_MASTER_COMMAND, ICW1_INIT | ICW1_ICW4);
	outb(PIC_SLAVE_COMMAND, ICW1_INIT | ICW1_ICW4);

  // ICW2: vector offset
	outb(PIC_MASTER_DATA, T_IRQ0);
	outb(PIC_SLAVE_DATA, T_IRQ0 + 8);

  // ICW3: slave
	outb(PIC_MASTER_DATA, (1 << IRQ_SLAVE));
	outb(PIC_SLAVE_DATA, IRQ_SLAVE);
	
  // ICW4
	outb(PIC_MASTER_DATA, ICW4_AUTO | ICW4_8086);
	outb(PIC_SLAVE_DATA, ICW4_AUTO | ICW4_8086);

  i8259_set_mask(i8259_mask);
}

static void
i8259_eoi(int irq)
{
  if (irq >= 8)
    outb(PIC_SLAVE_COMMAND, PIC_EOI);
  
  outb(PIC_MASTER_COMMAND, PIC_EOI);
}

#define PIT_DATA0   0x40
#define PIT_COMMAND 0x43

#define PIT_FREQ      1193182U
#define PIT_DIV(x)    ((PIT_FREQ+(x)/2)/(x))

#define PIT_SEL0      0x00    // select counter 0
#define PIT_RATEGEN   0x04    // mode 2, rate generator
#define PIT_16BIT     0x30    // r/w counter 16 bits, LSB first

void
pit_init(void)
{
  outb(PIT_COMMAND, PIT_SEL0 | PIT_RATEGEN | PIT_16BIT);
  outb(PIT_DATA0, PIT_DIV(TICKS_PER_SECOND) % 256);
  outb(PIT_DATA0, PIT_DIV(TICKS_PER_SECOND) / 256);

  arch_interrupt_unmask(0);
}

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
  // TODO
  (void) irq;
  (void) cpu;
}

void
arch_interrupt_mask(int irq)
{
  i8259_set_mask(i8259_mask | (1 << irq));
}

void
arch_interrupt_unmask(int irq)
{
  i8259_set_mask(i8259_mask & ~(1 << irq));
}

void
arch_interrupt_eoi(int irq)
{
  i8259_eoi(irq);
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

  i8259_init();

  pit_init();

  arch_interrupt_init_percpu();
}

void
arch_interrupt_init_percpu(void)
{
  lidt(&idtr);
}
