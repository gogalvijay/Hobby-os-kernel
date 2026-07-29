#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

void pmm_init(struct limine_memmap_response *memmap);
void *boot_alloc(size_t n);
void pmm_dump(void);
size_t pmm_boot_alloc_region(void);
uint64_t pmm_boot_alloc_next_free(void);

#endif
