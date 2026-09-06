#include "timer.h"
#include "pic.h"
#include "context_switch.h"
#include "scheduler.h"
#include "kprintf.h"

static volatile uint64_t g_ticks = 0;

uint64_t timer_get_ticks(void) {
    return g_ticks;
}

void timer_tick_handler(void) {
    g_ticks++;

    pic_send_eoi(0);

    if (g_ticks % TIME_SLICE_TICKS != 0) {
        return;
    }

    if (current_task == NULL) {
        return;
    }

    struct task *next = scheduler_pick_next(current_task);
    if (next == current_task) {
        return;
    }

    struct task *prev = current_task;

    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_RUNNABLE;
    }
    next->state = TASK_RUNNING;

    current_task = next;

    //context_switch(prev, next);
    scheduler_switch_to(prev, next);
}
