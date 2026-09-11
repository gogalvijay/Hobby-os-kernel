#include <stddef.h>
#include "scheduler.h"
#include "kprintf.h"

extern struct task *task_table_ptr(size_t index);
extern size_t task_table_size(void);

static struct task *idle_task = NULL;

#include "paging.h"
#include "context_switch.h"

#include "gdt.h"

void scheduler_switch_to(struct task *old, struct task *new) {
    if (new->pml4 != NULL) {
        vmm_switch_address_space(new->pml4);
    }
    if (new->kstack_top != NULL) {
        tss_set_rsp0((uint64_t)new->kstack_top);
    }
    context_switch(old, new);
}


void scheduler_set_idle_task(struct task *t) {
    idle_task = t;
}

static size_t index_of(struct task *t) {
    size_t n = task_table_size();
    for (size_t i = 0; i < n; i++) {
        if (task_table_ptr(i) == t) {
            return i;
        }
    }
    return 0;
}

void scheduler_init(void) {
    idle_task = NULL;
}

/*struct task *scheduler_pick_next(struct task *current) {
    size_t n = task_table_size();
    if (n == 0) {
        return current;
    }

    size_t start = (current != NULL) ? index_of(current) : 0;

    for (size_t offset = 1; offset <= n; offset++) {
        size_t idx = (start + offset) % n;
        struct task *t = task_table_ptr(idx);

        if (t == NULL) {
            continue;
        }
        if (t->state == TASK_RUNNABLE) {
            return t;
        }
    }

    if (current != NULL && current->state == TASK_RUNNABLE) {
        return current;
    }

    if (idle_task != NULL) {
        return idle_task;
    }

    return current;
}*/


struct task *scheduler_pick_next(struct task *current) {
    size_t n = task_table_size();
    if (n == 0) {
        return current;
    }

    uint64_t f = task_table_lock_acquire();

    size_t start = (current != NULL) ? index_of(current) : 0;
    struct task *found = NULL;

    for (size_t offset = 1; offset <= n; offset++) {
        size_t idx = (start + offset) % n;
        struct task *t = task_table_ptr(idx);
        if (t == NULL) {
            continue;
        }
        if (t->state == TASK_RUNNABLE) {
            found = t;
            break;
        }
    }

    if (found == NULL) {
        if (current != NULL && current->state == TASK_RUNNABLE) {
            found = current;
        } else if (idle_task != NULL) {
            found = idle_task;
        } else {
            found = current;
        }
    }

    task_table_lock_release(f);
    return found;
}
