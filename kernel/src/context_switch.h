#ifndef CONTEXT_SWITCH_H
#define CONTEXT_SWITCH_H

#include "task.h"

void context_switch(struct task *old, struct task *new);


//build fake initial stack frame on t kernel stack so that the first contextswitch into t lands inside entry  if it had just been called.
void task_stack_init(struct task *t, void (*entry)(void));

#endif
