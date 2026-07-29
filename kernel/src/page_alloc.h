#ifndef PA_H
#define PA_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

struct PageInfo {
    struct PageInfo *pp_link;
    uint16_t pp_ref;
};

void frame_alloc_init(struct limine_memmap_response *memmap);
struct PageInfo *page_alloc(int flags);
void page_free(struct PageInfo *pp);
uint64_t page2pa(struct PageInfo *pp);
struct PageInfo *pa2page(uint64_t pa);
size_t count_free(void);

#endif
