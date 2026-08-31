#include "timer.h"
#include "pic.h"
#include "context_switch.h"
#include "kprintf.h"

static volatile uint64_t g_ticks = 0;

static struct task *g_task_a;
static struct task *g_task_b;

void timer_set_round_robin_tasks(struct task *a, struct task *b) {
    g_task_a = a;
    g_task_b = b;
}

static struct task *pick_next_task(void) {
    if (current_task == g_task_a) {
        return g_task_b;
    }
    return g_task_a;
}

uint64_t timer_get_ticks(void) {
    return g_ticks;
}

void timer_tick_handler(void) {
    g_ticks++;

    pic_send_eoi(0);

    if (g_ticks % TIME_SLICE_TICKS != 0) {
        return;   
    }

    if (g_task_a == NULL || g_task_b == NULL || current_task == NULL) {
        return;  
    }

    struct task *next = pick_next_task();
    if (next == current_task) {
        return;
    }

    struct task *prev = current_task;
    current_task = next;

    context_switch(prev, next);

}
