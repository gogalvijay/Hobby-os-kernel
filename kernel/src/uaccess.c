#include "uaccess.h"
#include "kprintf.h"

#define PGSIZE 4096
#define KERNEL_PML4_START 256 //match pagin.c

static uint64_t rounddown(uint64_t v, uint64_t align) {
    return v & ~(align - 1);
}

bool uva_check_range(page_table_t *pml4, uint64_t uva, uint64_t len, bool need_write) {
    if (len == 0) {
        return true; 
    }

    // overflow check: uva + len must not wrap around
    uint64_t end = uva + len; 
    if (end < uva) {
        return false;
    }
    uint64_t last_byte = end - 1;

    // check 1: shape + zone
    if (!is_canonical(uva) || !is_canonical(last_byte)) {
        return false;
    }
    if (pml4_index(uva) >= KERNEL_PML4_START || pml4_index(last_byte) >= KERNEL_PML4_START) {
        return false;
    }

    // checks 2 and 3: walk every page the range touches
    uint64_t page_start = rounddown(uva, PGSIZE);
    uint64_t page_end_excl = rounddown(last_byte, PGSIZE) + PGSIZE;

    for (uint64_t va = page_start; va < page_end_excl; va += PGSIZE) {
        page_table_entry *pte = vmm_walk(pml4, va, false); // false: never create new mappings here
        if (pte == NULL || !present(*pte)) {
            return false;
        }
        if (!user_accessible(*pte)) {
            return false;
        }
        if (need_write && !writable(*pte)) {
            return false;
        }
    }

    return true;
}
