#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

void pmm_init(struct limine_memmap_response *memmap);
void *boot_alloc(size_t n);
void pmm_dump(void);

#endif
