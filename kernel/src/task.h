#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "paging.h"

struct task {
    uint64_t task_id;
    page_table_t *pml4;
};

struct task *task_create(void);

#endif
