#include "elf.h"
#include "page_alloc.h"
#include "hhdm.h"
#include "memory.h"
#include "kprintf.h"

#define PGSIZE 4096

static uint64_t rounddown(uint64_t v, uint64_t align) { return v & ~(align - 1); }
static uint64_t roundup(uint64_t v, uint64_t align)   { return (v + align - 1) & ~(align - 1); }

int elf_load(page_table_t *pml4, const void *elf_image, uint64_t *entry_out) {
    const uint8_t *base = (const uint8_t *)elf_image;
    const struct elf64_ehdr *eh = (const struct elf64_ehdr *)base;

    uint32_t magic = *(const uint32_t *)eh->e_ident;
    if (magic != ELF_MAGIC) {
        kprintf("elf_load: bad magic\n");
        return 0;
    }
    if (eh->e_ident[4] != 2) { 
        kprintf("elf_load: not 64-bit\n");
        return 0;
    }

    const struct elf64_phdr *ph =
        (const struct elf64_phdr *)(base + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const struct elf64_phdr *seg = &ph[i];
        if (seg->p_type != PT_LOAD) {
            continue;
        }

        uint64_t vstart = rounddown(seg->p_vaddr, PGSIZE);
        uint64_t vend   = roundup(seg->p_vaddr + seg->p_memsz, PGSIZE);

        uint64_t flags = (1ULL << 0) | (1ULL << 1); 

        for (uint64_t va = vstart; va < vend; va += PGSIZE) {
            page_table_entry *existing = vmm_walk(pml4, va, false);
            if (existing == NULL || !present(*existing)) {
                struct PageInfo *pp = page_alloc(1);
                if (pp == NULL) {
                    kprintf("elf_load: out of memory\n");
                    return 0;
                }
                vmm_map(pml4, va, page2pa(pp), flags);
            }
        }

        const uint8_t *src = base + seg->p_offset;
        uint8_t *dst = (uint8_t *)seg->p_vaddr; 
        memcpy(dst, src, seg->p_filesz);
    }

    *entry_out = eh->e_entry;
    return 1;
}
