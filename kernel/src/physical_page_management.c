#include "physical_page_management.h"
#include "kprintf.h"

static struct limine_memmap_response *memmap;
static size_t current_region;   // //first usable index in memap_entry
static uint64_t next_free;      // base of that usable entry

static const char *memmap_type_str(uint64_t type) {
    switch (type) {
        case 0: return "USABLE";
        case 1: return "RESERVED";
        case 2: return "ACPI_RECLAIMABLE";
        case 3: return "ACPI_NVS";
        case 4: return "BAD_MEMORY";
        case 5: return "BOOTLOADER_RECLAIMABLE";
        case 6: return "KERNEL_AND_MODULES";
        case 7: return "FRAMEBUFFER";
        case 8: return "EFI_RECLAIMABLE";
        default: return "UNKNOWN";
    }
}

static int region_is_usable(struct limine_memmap_entry *e) {
    return e->type == LIMINE_MEMMAP_USABLE;
}

static size_t find_next_usable_region(size_t start_index) {
    size_t found_index = memmap->entry_count;

    for (size_t i = start_index; i < memmap->entry_count; i++) {
        if (region_is_usable(memmap->entries[i])) {
            found_index = i;
            break;
        }
    }

    return found_index;
}

void pmm_init(struct limine_memmap_response *m) {
    memmap = m;
    next_free = 0;

    size_t first_usable_index = find_next_usable_region(0);

    if (first_usable_index == memmap->entry_count) {
        kprintf("pmm_init: no usable memory found!\n");
        current_region = memmap->entry_count;
        return;
    }

    current_region = first_usable_index;
    next_free = memmap->entries[current_region]->base;
}

void *boot_alloc(size_t n) {
    
     /* think of this like u have first usable rgion allocate as much as possible in that memory if not go to next entry and do the same*/	
     while (current_region < memmap->entry_count) {
        struct limine_memmap_entry *entry = memmap->entries[current_region];
        uint64_t region_end = entry->base + entry->length;
      
        if (next_free + n <= region_end) {
            uint64_t addr = next_free;
            next_free += n;
            return (void *)addr;
        }

        current_region = find_next_usable_region(current_region + 1);

        if (current_region < memmap->entry_count) {
            next_free = memmap->entries[current_region]->base;
        }
    }

    return NULL;
}

void pmm_dump(void) {
    kprintf("PMM memmap entries=%ld\n", (int64_t)memmap->entry_count);
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        kprintf("region=%ld base=%lx length=%lx type=%d type=%s\n",
                (int64_t)i, entry->base, entry->length, entry->type,
                memmap_type_str(entry->type));
    }
}
