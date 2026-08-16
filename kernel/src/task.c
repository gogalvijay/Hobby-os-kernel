#include <stddef.h>
#include "task.h"
#include "page_alloc.h"
#include "paging.h"
#include "hhdm.h"
#include "kprintf.h"

static uint64_t next_task_id = 1;

struct task *task_create(void) {
    page_table_t *pml4 = vmm_new_address_space();
    if (pml4 == NULL) {
        kprintf("task_create: failed to allocate address space\n");
        return NULL;
    }

    struct PageInfo *pp = page_alloc(1);
    if (pp == NULL) {
        kprintf("task_create: out of memory for task struct\n");
        return NULL;
    }

    struct task *t = (struct task *)phys_to_virt(page2pa(pp));
    t->task_id = next_task_id++;
    t->pml4 = pml4;

    return t;
}
