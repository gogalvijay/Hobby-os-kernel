#include <stddef.h>
#include "scheduler.h"
#include "kprintf.h"

extern struct task *task_table_ptr(size_t index);
extern size_t task_table_size(void);

static struct task *idle_task = NULL;

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

struct task *scheduler_pick_next(struct task *current) {
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
}
