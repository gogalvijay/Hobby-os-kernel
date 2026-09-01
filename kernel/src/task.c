#include <stddef.h>
#include "task.h"
#include "context_switch.h"
#include "page_alloc.h"
#include "paging.h"
#include "hhdm.h"
#include "kprintf.h"

static struct task task_table[MAX_TASKS];
static struct task *task_free_list;

struct task *current_task = NULL;

void task_table_init(void) {
    task_free_list = NULL;

    for (int i = MAX_TASKS - 1; i >= 0; i--) {
        task_table[i].task_id = (uint64_t)i;
        task_table[i].pml4 = NULL;
        task_table[i].state = TASK_UNUSED;
        task_table[i].next_free = task_free_list;
        task_table[i].context_rsp = 0;
        task_table[i].kstack = NULL;
        task_table[i].kstack_top = NULL;
        task_free_list = &task_table[i];
    }

    kprintf("task_table_init: %d slots ready\n", MAX_TASKS);
}

static struct task *task_slot_alloc(void) {
    if (task_free_list == NULL) {
        kprintf("task_slot_alloc: no free task slots\n");
        return NULL;
    }
    struct task *t = task_free_list;
    task_free_list = t->next_free;
    t->next_free = NULL;
    return t;
}

static void task_slot_free(struct task *t) {
    t->state = TASK_UNUSED;
    t->next_free = task_free_list;
    task_free_list = t;
}

static int task_kstack_alloc(struct task *t) {
    struct PageInfo *pp = page_alloc(1);
    if (pp == NULL) {
        return 0;
    }
    uint8_t *base = (uint8_t *)phys_to_virt(page2pa(pp));
    t->kstack = base;
    t->kstack_top = base + 4096;
    return 1;
}

struct task *task_create(void) {
    struct task *t = task_slot_alloc();
    if (t == NULL) {
        return NULL;
    }

    page_table_t *pml4 = vmm_new_address_space();
    if (pml4 == NULL) {
        kprintf("task_create: failed to allocate address space\n");
        task_slot_free(t);
        return NULL;
    }

    if (!task_kstack_alloc(t)) {
        kprintf("task_create: failed to allocate kernel stack\n");
        task_slot_free(t);
        return NULL;
    }

    t->pml4 = pml4;
    t->state = TASK_RUNNABLE;
    t->context_rsp = 0;

    kprintf("task_create: task_id=%ld pml4=%lx kstack_top=%lx state=RUNNABLE\n",
            (int64_t)t->task_id, (uint64_t)t->pml4, (uint64_t)t->kstack_top);

    return t;
}

struct task *task_create_kernel(void (*entry)(void)) {
    struct task *t = task_slot_alloc();
    if (t == NULL) {
        return NULL;
    }

    if (!task_kstack_alloc(t)) {
        kprintf("task_create_kernel: failed to allocate kernel stack\n");
        task_slot_free(t);
        return NULL;
    }

    t->pml4 = get_current_pml4();  
    t->state = TASK_RUNNABLE;

    task_stack_init(t, entry);

    kprintf("task_create_kernel: task_id=%ld kstack_top=%lx state=RUNNABLE\n",
            (int64_t)t->task_id, (uint64_t)t->kstack_top);

    return t;
}

void task_destroy(struct task *t) {
    if (t == NULL) {
        return;
    }
    if (t->state == TASK_UNUSED) {
        kprintf("task_destroy: double free of task_id=%ld\n", (int64_t)t->task_id);
        return;
    }

    t->pml4 = NULL;
    t->kstack = NULL;
    t->kstack_top = NULL;
    t->context_rsp = 0;

    task_slot_free(t);

    kprintf("task_destroy: task_id=%ld slot freed\n", (int64_t)t->task_id);
}

static const char *task_state_str(enum task_state s) {
    switch (s) {
        case TASK_UNUSED:   return "UNUSED";
        case TASK_RUNNABLE:  return "RUNNABLE";
        case TASK_RUNNING:   return "RUNNING";
        case TASK_BLOCKED:   return "BLOCKED";
        case TASK_DEAD:      return "DEAD";
        default:             return "UNKNOWN";
    }
}

void task_dump(void) {
    kprintf("task table (%d slots):\n", MAX_TASKS);
    for (int i = 0; i < MAX_TASKS; i++) {
        struct task *t = &task_table[i];
        if (t->state == TASK_UNUSED) {
            continue;
        }
        kprintf("  [%d] task_id=%ld state=%s pml4=%lx kstack_top=%lx\n",
                i, (int64_t)t->task_id, task_state_str(t->state),
                (uint64_t)t->pml4, (uint64_t)t->kstack_top);
    }
}

extern void task_entry_trampoline(void);

void task_stack_init(struct task *t, void (*entry)(void)) {
    uint64_t *sp = (uint64_t *)t->kstack_top;

    sp -= 1;
    *sp = (uint64_t)task_entry_trampoline;

    sp -= 6;
    sp[0] = 0;                  // r15
    sp[1] = 0;                  // r14
    sp[2] = 0;                  // r13
    sp[3] = (uint64_t)entry;    // r12 
    sp[4] = 0;                  // rbp
    sp[5] = 0;                  // rbx

    t->context_rsp = (uint64_t)sp;
}

struct task *task_table_ptr(size_t index) {
    if (index >= MAX_TASKS) {
        return NULL;
    }
    return &task_table[index];
}

size_t task_table_size(void) {
    return MAX_TASKS;
}
