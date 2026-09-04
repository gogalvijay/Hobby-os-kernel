#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "kprintf.h"
#include "paging.h"
#include "hhdm.h"
#include "page_alloc.h"

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define KERNEL_PML4_START 256   // indices 256-511 = higher half = kernel space

#include "page_alloc.h"
#include "memory.h"

page_table_t *vmm_copy_address_space(page_table_t *parent_pml4) {
    struct PageInfo *pml4_pp = page_alloc(1);
    if (pml4_pp == NULL) {
        return NULL;
    }
    page_table_t *child_pml4 = (page_table_t *)phys_to_virt(page2pa(pml4_pp));

    for (int i = KERNEL_PML4_START; i < 512; i++) {
        child_pml4->entries[i] = parent_pml4->entries[i];
    }

    for (int i4 = 0; i4 < KERNEL_PML4_START; i4++) {
        if (!present(parent_pml4->entries[i4])) {
            continue;
        }
        page_table_t *parent_pdpt =
            (page_table_t *)phys_to_virt(pte_get_addr(parent_pml4->entries[i4]));

        for (int i3 = 0; i3 < 512; i3++) {
            if (!present(parent_pdpt->entries[i3])) {
                continue;
            }
            page_table_t *parent_pd =
                (page_table_t *)phys_to_virt(pte_get_addr(parent_pdpt->entries[i3]));

            for (int i2 = 0; i2 < 512; i2++) {
                if (!present(parent_pd->entries[i2])) {
                    continue;
                }
                page_table_t *parent_pt =
                    (page_table_t *)phys_to_virt(pte_get_addr(parent_pd->entries[i2]));

                for (int i1 = 0; i1 < 512; i1++) {
                    page_table_entry parent_pte = parent_pt->entries[i1];
                    if (!present(parent_pte)) {
                        continue;
                    }

                    uint64_t vaddr = ((uint64_t)i4 << 39)
                                    | ((uint64_t)i3 << 30)
                                    | ((uint64_t)i2 << 21)
                                    | ((uint64_t)i1 << 12);

                    uint64_t parent_phys = pte_get_addr(parent_pte);
                    uint64_t flags = parent_pte & 0xFFFULL;

                    struct PageInfo *new_pp = page_alloc(0);
                    if (new_pp == NULL) {
                        return NULL;
                    }
                    uint64_t new_phys = page2pa(new_pp);

                    memcpy(phys_to_virt(new_phys), phys_to_virt(parent_phys), 4096);

                    vmm_map(child_pml4, vaddr, new_phys, flags);
                }
            }
        }
    }

    return child_pml4;
}


bool present(page_table_entry pte){
	return pte&1;
}

bool writable(page_table_entry pte){
	uint64_t pos=1;
	return (pos<<1)&pte;
}

bool user_accessible(page_table_entry pte){
	uint64_t pos=1;
	return (pos<<2)&pte;
}

bool accessed(page_table_entry pte){
	uint64_t pos=1;
	return (pos<<5)&pte;
}

bool dirty(page_table_entry pte){
	uint64_t pos=1;
	return (pos<<6)&pte;
}

bool no_execute(page_table_entry pte){
	uint64_t pos=1;
	return (pos<<63)&pte;
}

uint64_t pte_get_addr(page_table_entry pte){
	uint64_t temp = 1;
	uint64_t addr = 0;

	for (size_t pos = 12; pos <= 51; pos++) {
		if ((temp << pos) & pte) {
			addr |= (temp << pos);
		}
	}

	return addr;
}

page_table_entry pte_make(uint64_t phys_addr, uint64_t flags){
	uint64_t temp = 1;
	uint64_t entry = flags;

	for (size_t pos = 12; pos <= 51; pos++) {
		if ((temp << pos) & phys_addr) {
			entry |= (temp << pos);
		}
	}

	return entry;
}


bool is_canonical(uint64_t vaddr){
	uint64_t temp = 1;
	uint64_t first_bit = (vaddr & (temp << 47)) ? 1 : 0;

	for (size_t pos = 47; pos <= 63; pos++) {
		uint64_t this_bit = (vaddr & (temp << pos)) ? 1 : 0;
		if (this_bit != first_bit) {
			return false;
		}
	}

	return true;
}

uint16_t pml4_index(uint64_t vaddr){
	uint64_t temp = 1;
	uint16_t index = 0;

	for (size_t pos = 47; pos >= 39; pos--) {
		index = index << 1;
		if ((temp << pos) & vaddr) {
			index = index | 1;
		}
		if (pos == 39) {
			break;
		}
	}

	return index;
}

uint16_t pdpt_index(uint64_t vaddr){
	uint64_t temp = 1;
	uint16_t index = 0;

	for (size_t pos = 38; pos >= 30; pos--) {
		index = index << 1;
		if ((temp << pos) & vaddr) {
			index = index | 1;
		}
		if (pos == 30) {
			break;
		}
	}

	return index;
}

uint16_t pd_index(uint64_t vaddr){
	uint64_t temp = 1;
	uint16_t index = 0;

	for (size_t pos = 29; pos >= 21; pos--) {
		index = index << 1;
		if ((temp << pos) & vaddr) {
			index = index | 1;
		}
		if (pos == 21) {
			break;
		}
	}

	return index;
}

uint16_t pt_index(uint64_t vaddr){
	uint64_t temp = 1;
	uint16_t index = 0;

	for (size_t pos = 20; pos >= 12; pos--) {
		index = index << 1;
		if ((temp << pos) & vaddr) {
			index = index | 1;
		}
		if (pos == 12) {
			break;
		}
	}

	return index;
}


//limine already created page table iam extending that table and using for my kernel
page_table_t *get_current_pml4(void){
	uint64_t cr3;
	__asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
	uint64_t phys = cr3 & PTE_ADDR_MASK;
	return (page_table_t *)phys_to_virt(phys);

	//currently cr3 holds the active page table running in order for vmm_walk,vmm_map i need top level so iam using cr3 where it points and extend it out
}

//given a virtual address and the top-level page table return a pointer to the Page Table Entry that would map that address
page_table_entry *vmm_walk(page_table_t *pml4, uint64_t vaddr, bool create){
	uint16_t idx4 = pml4_index(vaddr);

	if (!present(pml4->entries[idx4])) {
		if (!create) {
			return NULL;
		}
		struct PageInfo *pp = page_alloc(1);
		if (pp == NULL) {
			return NULL;
		}
		uint64_t new_phys = page2pa(pp);
		uint64_t flags = 0;
		flags = flags | (1ULL << 0);
		flags = flags | (1ULL << 1);
		flags = flags | (1ULL << 2); // user-accessible: needed so ring-3 can walk through this table
		pml4->entries[idx4] = pte_make(new_phys, flags);
	}

	uint64_t pdpt_phys = pte_get_addr(pml4->entries[idx4]);
	page_table_t *pdpt = (page_table_t *)phys_to_virt(pdpt_phys);
	uint16_t idx3 = pdpt_index(vaddr);

	if (!present(pdpt->entries[idx3])) {
		if (!create) {
			return NULL;
		}
		struct PageInfo *pp = page_alloc(1);
		if (pp == NULL) {
			return NULL;
		}
		uint64_t new_phys = page2pa(pp);
		uint64_t flags = 0;
		flags = flags | (1ULL << 0);
		flags = flags | (1ULL << 1);
		flags = flags | (1ULL << 2);
		pdpt->entries[idx3] = pte_make(new_phys, flags);
	}

	uint64_t pd_phys = pte_get_addr(pdpt->entries[idx3]);
	page_table_t *pd = (page_table_t *)phys_to_virt(pd_phys);
	uint16_t idx2 = pd_index(vaddr);

	if (!present(pd->entries[idx2])) {
		if (!create) {
			return NULL;
		}
		struct PageInfo *pp = page_alloc(1);
		if (pp == NULL) {
			return NULL;
		}
		uint64_t new_phys = page2pa(pp);
		uint64_t flags = 0;
		flags = flags | (1ULL << 0);
		flags = flags | (1ULL << 1);
		flags = flags | (1ULL << 2);
		pd->entries[idx2] = pte_make(new_phys, flags);
	}

	uint64_t pt_phys = pte_get_addr(pd->entries[idx2]);
	page_table_t *pt = (page_table_t *)phys_to_virt(pt_phys);
	uint16_t idx1 = pt_index(vaddr);

	return &pt->entries[idx1];
}

//map the virtual addr to physical addr(actual translation)
void vmm_map(page_table_t *pml4, uint64_t vaddr, uint64_t phys_addr, uint64_t flags){
	page_table_entry *pte = vmm_walk(pml4, vaddr, true);
	if (pte == NULL) {
		return;
	}
	*pte = pte_make(phys_addr, flags);
}

void invlpg(uint64_t vaddr){
	__asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
}

//unmap 
void vmm_unmap(page_table_t *pml4, uint64_t vaddr){
	page_table_entry *pte = vmm_walk(pml4, vaddr, false);
	if (pte == NULL) {
		return;
	}
	if (!present(*pte)) {
		return;
	}
	*pte = 0;
	invlpg(vaddr);
}

page_table_t *vmm_new_address_space(void){
	//day 28 allocate new page table for process and copy and switch cr3
	struct PageInfo *pp = page_alloc(1); 
	if (pp == NULL) {
		return NULL;
	}

	page_table_t *new_pml4 = (page_table_t *)phys_to_virt(page2pa(pp));
	page_table_t *kernel_pml4 = get_current_pml4();

	for (int i = KERNEL_PML4_START; i < 512; i++) {
		new_pml4->entries[i] = kernel_pml4->entries[i];
	}

	return new_pml4;
}

void vmm_switch_address_space(page_table_t *pml4){
	//swith cr3 for ay28
	uint64_t phys = virt_to_phys((void *)pml4);
	__asm__ volatile ("mov %0, %%cr3" :: "r"(phys) : "memory");
}


//walk-find the slot
//map-write value in to slot
//unmap-clear value in to slot
