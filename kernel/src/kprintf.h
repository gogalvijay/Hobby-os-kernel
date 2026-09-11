#ifndef KPRINTF_H
#define KPRINTF_H

#include <stdarg.h>

void kprintf(const char *fmt, ...);
void kprintf_lock_init(void);

#endif
