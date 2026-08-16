#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
typedef uint64_t page_table_entry;
typedef struct {
    page_table_entry entries[512];
} page_table_t;
bool present(page_table_entry pte);
bool writable(page_table_entry pte);
bool user_accessible(page_table_entry pte);
bool accessed(page_table_entry pte);
bool dirty(page_table_entry pte);
bool no_execute(page_table_entry pte);
uint64_t pte_get_addr(page_table_entry pte);
page_table_entry pte_make(uint64_t phys_addr, uint64_t flags);
uint16_t pml4_index(uint64_t vaddr);
uint16_t pdpt_index(uint64_t vaddr);
uint16_t pd_index(uint64_t vaddr);
uint16_t pt_index(uint64_t vaddr);
bool is_canonical(uint64_t vaddr);
page_table_entry *vmm_walk(page_table_t *pml4, uint64_t vaddr, bool create);
void vmm_map(page_table_t *pml4, uint64_t vaddr, uint64_t phys_addr, uint64_t flags);
void vmm_unmap(page_table_t *pml4, uint64_t vaddr);
void invlpg(uint64_t vaddr);
page_table_t *get_current_pml4(void);
page_table_t *vmm_new_address_space(void);
void vmm_switch_address_space(page_table_t *pml4);
#endif
