#include <stddef.h>
#include "task.h"
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
        task_free_list = &task_table[i];
    }

    kprintf("task_table_init: %d slots ready\n", MAX_TASKS);
}

struct task *task_create(void) {
    if (task_free_list == NULL) {
        kprintf("task_create: no free task slots\n");
        return NULL;
    }

    struct task *t = task_free_list;
    task_free_list = t->next_free;
    t->next_free = NULL;

    page_table_t *pml4 = vmm_new_address_space();
    if (pml4 == NULL) {
        kprintf("task_create: failed to allocate address space\n");
        t->state = TASK_UNUSED;
        t->next_free = task_free_list;
        task_free_list = t;
        return NULL;
    }

    t->pml4 = pml4;
    t->state = TASK_RUNNABLE;

    kprintf("task_create: task_id=%ld pml4=%lx state=RUNNABLE\n",
            (int64_t)t->task_id, (uint64_t)t->pml4);

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
    t->state = TASK_UNUSED;

    t->next_free = task_free_list;
    task_free_list = t;

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
        kprintf("  [%d] task_id=%ld state=%s pml4=%lx\n",
                i, (int64_t)t->task_id, task_state_str(t->state), (uint64_t)t->pml4);
    }
}
