#include <stdint.h>
#include <stddef.h>
#include "kheap.h"
#include "page_alloc.h"
#include "paging.h"
#include "hhdm.h"
#include "memlayout.h"
#include "kprintf.h"
#include "spinlock.h"

static spinlock_t kheap_lock;

#define PGSIZE 4096
#define ALIGN 16

struct block_header {
    size_t size;
    int free;
    struct block_header *next;
};

static uint64_t heap_next = KHEAP_BASE;
static uint64_t heap_end = KHEAP_BASE;
static struct block_header *free_list = NULL;

static uint64_t roundup(uint64_t val, uint64_t align) {
    return (val + align - 1) & ~(align - 1);
}

static int heap_extend(size_t npages) {
    for (size_t i = 0; i < npages; i++) {
        if (heap_next >= KHEAP_BASE + KHEAP_SIZE) {
            return 0;
        }

        struct PageInfo *pp = page_alloc(1);
        if (pp == NULL) {
            return 0;
        }

        uint64_t phys = page2pa(pp);
        uint64_t flags = 0;
        flags = flags | (1ULL << 0);
        flags = flags | (1ULL << 1);

        vmm_map(get_current_pml4(), heap_next, phys, flags);

        heap_next += PGSIZE;
    }

    heap_end = heap_next;
    return 1;
}

void kheap_init(void) {
    heap_next = KHEAP_BASE;
    heap_end = KHEAP_BASE;
    free_list = NULL;
}

static struct block_header *find_free_block(size_t size) {
    struct block_header *cur = free_list;
    while (cur != NULL) {
        if (cur->free && cur->size >= size) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static void split_block(struct block_header *b, size_t size) {
    size_t min_split = sizeof(struct block_header) + ALIGN;

    if (b->size >= size + min_split) {
        uint8_t *base = (uint8_t *)b;
        struct block_header *newb = (struct block_header *)(base + sizeof(struct block_header) + size);

        newb->size = b->size - size - sizeof(struct block_header);
        newb->free = 1;
        newb->next = b->next;

        b->size = size;
        b->next = newb;
    }
}

/*void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size = roundup(size, ALIGN);

    struct block_header *b = find_free_block(size);
    if (b != NULL) {
        split_block(b, size);
        b->free = 0;
        return (void *)((uint8_t *)b + sizeof(struct block_header));
    }

    size_t needed = size + sizeof(struct block_header);
    size_t npages = (needed + PGSIZE - 1) / PGSIZE;

    uint64_t new_block_addr = heap_next;

    if (!heap_extend(npages)) {
        return NULL;
    }

    struct block_header *nb = (struct block_header *)new_block_addr;
    nb->size = npages * PGSIZE - sizeof(struct block_header);
    nb->free = 0;
    nb->next = free_list;
    free_list = nb;

    split_block(nb, size);

    return (void *)((uint8_t *)nb + sizeof(struct block_header));
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    struct block_header *b = (struct block_header *)((uint8_t *)ptr - sizeof(struct block_header));
    b->free = 1;

    struct block_header *cur = free_list;
    while (cur != NULL) {
        if (cur->free && cur->next != NULL && cur->next->free) {
            uint8_t *cur_end = (uint8_t *)cur + sizeof(struct block_header) + cur->size;
            if (cur_end == (uint8_t *)cur->next) {
                cur->size += sizeof(struct block_header) + cur->next->size;
                cur->next = cur->next->next;
                continue;
            }
        }
        cur = cur->next;
    }
}*/



void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = roundup(size, ALIGN);

    uint64_t f = spinlock_acquire(&kheap_lock);

    struct block_header *b = find_free_block(size);
    if (b != NULL) {
        split_block(b, size);
        b->free = 0;
        spinlock_release(&kheap_lock, f);
        return (void *)((uint8_t *)b + sizeof(struct block_header));
    }

    size_t needed = size + sizeof(struct block_header);
    size_t npages = (needed + PGSIZE - 1) / PGSIZE;
    uint64_t new_block_addr = heap_next;

    if (!heap_extend(npages)) {
        spinlock_release(&kheap_lock, f);
        return NULL;
    }

    struct block_header *nb = (struct block_header *)new_block_addr;
    nb->size = npages * PGSIZE - sizeof(struct block_header);
    nb->free = 0;
    nb->next = free_list;
    free_list = nb;

    split_block(nb, size);

    spinlock_release(&kheap_lock, f);
    return (void *)((uint8_t *)nb + sizeof(struct block_header));
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    uint64_t f = spinlock_acquire(&kheap_lock);

    struct block_header *b = (struct block_header *)((uint8_t *)ptr - sizeof(struct block_header));
    b->free = 1;

    struct block_header *cur = free_list;
    while (cur != NULL) {
        if (cur->free && cur->next != NULL && cur->next->free) {
            uint8_t *cur_end = (uint8_t *)cur + sizeof(struct block_header) + cur->size;
            if (cur_end == (uint8_t *)cur->next) {
                cur->size += sizeof(struct block_header) + cur->next->size;
                cur->next = cur->next->next;
                continue;
            }
        }
        cur = cur->next;
    }

    spinlock_release(&kheap_lock, f);
}
