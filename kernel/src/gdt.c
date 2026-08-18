#include "gdt.h"
#include "kprintf.h"

#define GDT_ENTRIES 7  // null, kcode, kdata, udata, ucode, tss_lo, tss_hi

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdtr      gdtr;
static struct tss       kernel_tss;

static uint8_t rsp0_stack[16384] __attribute__((aligned(16)));

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran_hi) {
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].granularity = (gran_hi & 0xF0) | ((limit >> 16) & 0x0F);
    gdt[i].base_high   = (base >> 24) & 0xFF;
}

static void tss_set_entry(int i, uint64_t base, uint32_t limit) {
    struct tss_entry *e = (struct tss_entry *)&gdt[i];
    e->limit_low   = limit & 0xFFFF;
    e->base_low    = base & 0xFFFF;
    e->base_mid    = (base >> 16) & 0xFF;
    e->access      = 0x89;              
    e->granularity = (limit >> 16) & 0x0F;
    e->base_high   = (base >> 24) & 0xFF;
    e->base_upper  = (base >> 32) & 0xFFFFFFFF;
    e->reserved    = 0;
}

static inline void gdt_reload_segments(void) {
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        ::: "rax", "memory"
    );
}

void gdt_init(void) {
    gdt_set_entry(0, 0, 0,       0x00, 0x00);          
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);           
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);           
    gdt_set_entry(3, 0, 0xFFFFF, 0xF2, 0xC0);           
    gdt_set_entry(4, 0, 0xFFFFF, 0xFA, 0xA0);           

    kernel_tss.rsp0 = (uint64_t)(rsp0_stack + sizeof(rsp0_stack));
    kernel_tss.iopb_offset = sizeof(struct tss);        

    tss_set_entry(5, (uint64_t)&kernel_tss, sizeof(struct tss) - 1);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)&gdt;

    __asm__ volatile ("lgdt %0" :: "m"(gdtr));

    gdt_reload_segments();

    __asm__ volatile ("ltr %%ax" :: "a"((uint16_t)GDT_TSS_SEL));

    kprintf("gdt_init: loaded custom GDT, TSS rsp0=%lx\n", kernel_tss.rsp0);
}

void tss_set_rsp0(uint64_t rsp0) {
    kernel_tss.rsp0 = rsp0;
}


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
