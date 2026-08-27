#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "paging.h"

#define MAX_TASKS 64
#define TASK_KSTACK_PAGES 2
#define TASK_KSTACK_SIZE (TASK_KSTACK_PAGES * 4096)

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

    
    uint64_t context_rsp;     
    uint8_t *kstack;         
    uint8_t *kstack_top;      
};

void task_table_init(void);
struct task *task_create(void);
void task_destroy(struct task *t);
void task_dump(void);

// day 40-42
struct task *task_create_kernel(void (*entry)(void));

extern struct task *current_task;

#endif
