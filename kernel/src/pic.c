#include "pic.h"
#include "io.h"
#include "kprintf.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT   0x10
#define ICW1_ICW4   0x01
#define ICW4_8086   0x01

#define PIC_EOI 0x20

void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20);   
    io_wait();
    outb(PIC2_DATA, 0x28);   
    io_wait();

    outb(PIC1_DATA, 0x04);   
    io_wait();
    outb(PIC2_DATA, 0x02);   
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    outb(0x22, 0x70);
    outb(0x23, 0x01);

    kprintf("pic_remap: IRQ0-7 -> 0x20-0x27, IRQ8-15 -> 0x28-0x2F, IMCR forced to PIC mode\n");
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t irq_line = (irq < 8) ? irq : irq - 8;
    uint8_t value = inb(port) | (1 << irq_line);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t irq_line = (irq < 8) ? irq : irq - 8;
    uint8_t value = inb(port) & ~(1 << irq_line);
    outb(port, value);
}
