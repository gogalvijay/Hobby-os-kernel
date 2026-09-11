#include <stddef.h>
#include <stdint.h>
#include <limine.h>
#include "page_alloc.h"
#include "physical_page_management.h"
#include "hhdm.h"
#include "kprintf.h"
#include "spinlock.h"

static spinlock_t page_alloc_lock;


#define PGSIZE 4096
#define ALLOC_ZERO 1

static struct PageInfo *pages;
static size_t npages;
static struct PageInfo *page_free_list;

static uint64_t roundup(uint64_t addr, uint64_t align) {
    return (addr + align - 1) & ~(align - 1);
}

static uint64_t rounddown(uint64_t addr, uint64_t align) {
    return addr & ~(align - 1);
}

void frame_alloc_init(struct limine_memmap_response *memmap) {
    uint64_t max_addr = 0;
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            uint64_t end = e->base + e->length;
            if (end > max_addr) {
                max_addr = end;
            }
        }
    }

    npages = max_addr / PGSIZE;

    uint64_t pages_phys = (uint64_t)boot_alloc(npages * sizeof(struct PageInfo));
    if (pages_phys == 0 && npages != 0) {
        kprintf("frame_alloc_init: boot_alloc failed for pages[]\n");
        return;
    }

    pages = (struct PageInfo *)phys_to_virt(pages_phys);

    for (size_t i = 0; i < npages; i++) {
        pages[i].pp_link = NULL;
        pages[i].pp_ref = 0;
    }

    page_free_list = NULL;

    size_t consumed_region = pmm_boot_alloc_region();
    uint64_t consumed_frontier = pmm_boot_alloc_next_free();

    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t start = roundup(entry->base, PGSIZE);
        uint64_t end = rounddown(entry->base + entry->length, PGSIZE);

        if (i < consumed_region) {
            continue;
        }
        if (i == consumed_region && start < consumed_frontier) {
            start = roundup(consumed_frontier, PGSIZE);
        }

        for (uint64_t addr = start; addr < end; addr += PGSIZE) {
            size_t idx = addr / PGSIZE;
            if (idx >= npages) {
                continue;
            }
            pages[idx].pp_link = page_free_list;
            page_free_list = &pages[idx];
        }
    }

    size_t free_count = 0;
    for (struct PageInfo *p = page_free_list; p != NULL; p = p->pp_link) {
    	free_count++;
    }
    kprintf("frame_alloc_init: npages=%ld free=%ld\n", (int64_t)npages, (int64_t)free_count);



}


/*struct PageInfo *page_alloc(int flags) {
    if (page_free_list == NULL) {
        return NULL;
    }

    struct PageInfo *pp = page_free_list;
    page_free_list = pp->pp_link;
    pp->pp_link = NULL;

    if (flags & ALLOC_ZERO) {
        uint8_t *va = (uint8_t *)phys_to_virt(page2pa(pp));
        for (int i = 0; i < PGSIZE; i++) {
            va[i] = 0;
        }
    }

    return pp;
}*/

/*void page_free(struct PageInfo *pp) {
    if (pp->pp_ref != 0 || pp->pp_link != NULL) {
        kprintf("page_free: bad free ref=%d\n", pp->pp_ref);
        return;
    }
    pp->pp_link = page_free_list;
    page_free_list = pp;
}*/

uint64_t page2pa(struct PageInfo *pp) {
    return (uint64_t)(pp - pages) * PGSIZE;
}

struct PageInfo *pa2page(uint64_t pa) {
    return &pages[pa / PGSIZE];
}

/*size_t count_free(void) {
    size_t count = 0;
    for (struct PageInfo *p = page_free_list; p != NULL; p = p->pp_link) {
        count++;
    }
    return count;
}*/




struct PageInfo *page_alloc(int flags) {
    uint64_t f = spinlock_acquire(&page_alloc_lock);

    if (page_free_list == NULL) {
        spinlock_release(&page_alloc_lock, f);
        return NULL;
    }

    struct PageInfo *pp = page_free_list;
    page_free_list = pp->pp_link;
    pp->pp_link = NULL;

    spinlock_release(&page_alloc_lock, f);

    if (flags & ALLOC_ZERO) {
        uint8_t *va = (uint8_t *)phys_to_virt(page2pa(pp));
        for (int i = 0; i < PGSIZE; i++) {
            va[i] = 0;
        }
    }

    return pp;
}

void page_free(struct PageInfo *pp) {
    uint64_t f = spinlock_acquire(&page_alloc_lock);

    if (pp->pp_ref != 0 || pp->pp_link != NULL) {
        spinlock_release(&page_alloc_lock, f);
        kprintf("page_free: bad free ref=%d\n", pp->pp_ref);
        return;
    }
    pp->pp_link = page_free_list;
    page_free_list = pp;

    spinlock_release(&page_alloc_lock, f);
}

size_t count_free(void) {
    uint64_t f = spinlock_acquire(&page_alloc_lock);
    size_t count = 0;
    for (struct PageInfo *p = page_free_list; p != NULL; p = p->pp_link) {
        count++;
    }
    spinlock_release(&page_alloc_lock, f);
    return count;
}

