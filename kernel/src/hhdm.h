#ifndef HHDM_H
#define HHDM_H

#include <stdint.h>

extern uint64_t g_hhdm_offset;

void hhdm_init(uint64_t offset);

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + g_hhdm_offset);
}

static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - g_hhdm_offset;
}

#endif
