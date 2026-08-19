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
