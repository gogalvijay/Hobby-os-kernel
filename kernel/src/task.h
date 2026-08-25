#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "paging.h"

#define MAX_TASKS 64

enum task_state {
    TASK_UNUSED,
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD,
};

struct task {
    uint64_t task_id;
    page_table_t *pml4;
    enum task_state state;
    struct task *next_free;   
};

void task_table_init(void);
struct task *task_create(void);
void task_destroy(struct task *t);
void task_dump(void);

extern struct task *current_task;

#endif
