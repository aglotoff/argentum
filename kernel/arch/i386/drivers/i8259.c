#include <arch/i386/i8259.h>
#include <arch/i386/io.h>

enum {
  PIC1_CMD  = 0x20,
  PIC1_DATA = 0x21,
  PIC2_CMD  = 0xA0,
  PIC2_DATA = 0xA1,
};

enum {
  ICW1_IC4        = (1 << 0),   // PIC expects to recieve ICW4
  ICW1_SNGL       = (1 << 1),   // Single (cascade) mode
  ICW1_ADI        = (1 << 2),   // Call address interval is 4 (8)
  ICW1_LTIM       = (1 << 3),   // Operate in Level (Edge) Triggered mode
  ICW1_INIT       = (1 << 4),   // Initialization bit
};

enum {
  ICW4_8086       = (1 << 0),   // 8086/88 (MCS-80/85) mode
  ICW4_AEOI       = (1 << 1),   // Auto EOI
  ICW4_BUF_SLAVE  = (1 << 3),   // Buffered mode/slave
  ICW4_BUF_MASTER = (3 << 3),   // Buffered mode/master
  ICW4_SFNM       = (1 << 4),   // Special Fully Nested Mode
};

enum {
  OCW2_EOI        = (1 << 5),   // End of Interrupt (EOI) request
};

static const int PIC_IRQ_MAX = 8;

void
i8259_mask(int irq)
{
  if (irq >= PIC_IRQ_MAX)
    outb(PIC2_DATA, inb(PIC2_DATA) | (1 << (irq - PIC_IRQ_MAX)));
  else
    outb(PIC1_DATA, inb(PIC1_DATA) | (1 << irq));
}

void
i8259_unmask(int irq)
{
  if (irq >= PIC_IRQ_MAX)
    outb(PIC2_DATA, inb(PIC2_DATA) & ~(1 << (irq - PIC_IRQ_MAX)));
  else
    outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << irq));
}

void
i8259_init(uint8_t vector_base, int irq_cascade)
{
  // Disable all interrupts
  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);

  // ICW1: begin initialization, edge triggered, cascade mode, ICW4 present
  outb(PIC1_CMD, ICW1_INIT | ICW1_IC4);
  outb(PIC2_CMD, ICW1_INIT | ICW1_IC4);

  // ICW2: vector base address
  outb(PIC1_DATA, vector_base);
  outb(PIC2_DATA, vector_base + PIC_IRQ_MAX);

  // ICW3: 
  outb(PIC1_DATA, (1 << irq_cascade));
  outb(PIC2_DATA, irq_cascade);

  // ICW4: auto EOI, 8086 mode
  outb(PIC1_DATA, ICW4_AEOI | ICW4_8086);
  outb(PIC2_DATA, ICW4_AEOI | ICW4_8086);

  i8259_unmask(irq_cascade);
}

void
i8259_eoi(int irq)
{
  if (irq >= PIC_IRQ_MAX)
    outb(PIC2_CMD, OCW2_EOI);
  
  outb(PIC1_CMD, OCW2_EOI);
}
