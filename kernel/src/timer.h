#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "task.h"

#define TIME_SLICE_TICKS 20

void timer_tick_handler(void);
uint64_t timer_get_ticks(void);

#endif
