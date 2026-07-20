#ifndef IDT_H
#define IDT_H
#include <stdint.h>

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_low;    // bits 0-15 of handler address
    uint16_t selector;      // GDT code segment selector (e.g. 0x08)
    uint8_t  ist;           // bits 0-2: IST index, rest reserved (0)
    uint8_t  type_attr;     // present | DPL | gate type (0xE = interrupt, 0xF = trap)
    uint16_t offset_mid;    // bits 16-31 of handler address
    uint32_t offset_high;   // bits 32-63 of handler address
    uint32_t reserved;      // must be 0
} __attribute__((packed));

void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type_attr);
void idt_dump(void);

#endif
