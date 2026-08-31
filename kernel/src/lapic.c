#include <stdint.h>
#include "lapic.h"
#include "hhdm.h"
#include "paging.h"
#include "kprintf.h"

#define IA32_APIC_BASE_MSR 0x1B

#define LAPIC_REG_SVR       0x0F0
#define LAPIC_REG_LVT_LINT0 0x350

static volatile uint32_t *lapic_base;

static uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

static uint32_t lapic_read(uint32_t reg) {
    return lapic_base[reg / 4];
}

static void lapic_write(uint32_t reg, uint32_t value) {
    lapic_base[reg / 4] = value;
}

void lapic_enable(void) {
    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    uint64_t apic_phys = apic_base_msr & 0xFFFFF000ULL;

    apic_base_msr |= (1ULL << 11);
    wrmsr(IA32_APIC_BASE_MSR, apic_base_msr);

    uint64_t vaddr = (uint64_t)phys_to_virt(apic_phys);
    uint64_t flags = (1ULL << 0) | (1ULL << 1);
    vmm_map(get_current_pml4(), vaddr, apic_phys, flags);

    lapic_base = (volatile uint32_t *)vaddr;

    lapic_write(LAPIC_REG_SVR, lapic_read(LAPIC_REG_SVR) | 0x100 | 0xFF);

    lapic_write(LAPIC_REG_LVT_LINT0, 0x700);

    kprintf("lapic_enable: base=%lx, LINT0 set to ExtINT/unmasked\n", (uint64_t)apic_phys);
}
