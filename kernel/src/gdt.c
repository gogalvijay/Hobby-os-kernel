#include "gdt.h"
#include "kprintf.h"

static void gdt_read(struct gdtr *out) {
    __asm__ volatile ("sgdt %0" : "=m"(*out));
}

static uint32_t gdt_entry_base(struct gdt_entry *e) {
    return e->base_low | (e->base_mid << 16) | (e->base_high << 24);
}

static uint32_t gdt_entry_limit(struct gdt_entry *e) {
    return e->limit_low | ((e->granularity & 0x0F) << 16);
}

static void gdt_entry_print(uint32_t index, struct gdt_entry *e) {
    uint32_t base = gdt_entry_base(e);
    uint32_t limit = gdt_entry_limit(e);
    uint32_t flags = (e->granularity >> 4) & 0xF;

    kprintf("[%d] sel=0x%x base=0x%x limit=0x%x access=0x%x flags=0x%x\n",
            index, index * 8, base, limit, e->access, flags);
}

void gdt_dump(void) {
    struct gdtr r;
    gdt_read(&r);

    uint32_t num_entries = (r.limit + 1) / 8;
    kprintf("GDT base=0x%x limit=%d entries=%d\n",
            (uint32_t)r.base, r.limit, num_entries);

    struct gdt_entry *entries = (struct gdt_entry *)r.base;
    for (uint32_t i = 0; i < num_entries; i++) {
        gdt_entry_print(i, &entries[i]);
    }
}
