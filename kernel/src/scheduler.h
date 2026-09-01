#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"

void scheduler_init(void);
struct task *scheduler_pick_next(struct task *current);

#endif
