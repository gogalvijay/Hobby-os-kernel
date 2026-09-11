#include "spinlock.h"

#include <stdint.h>
#include <stdarg.h>
#include "serial.h"
#include "kprintf.h"


static spinlock_t kprintf_lock;
static int kprintf_lock_ready = 0;

void kprintf_lock_init(void) {
    spinlock_init(&kprintf_lock);
    kprintf_lock_ready = 1;
}



static char *uint_to_str(uint64_t value, unsigned base, char *buf, int buf_size) {
    static const char digits[] = "0123456789abcdef";
    int pos = buf_size - 1;
    buf[pos] = '\0';

    if (value == 0) {
        pos--;
        buf[pos] = '0';
        return &buf[pos];
    }

    while (value != 0 && pos > 0) {
        pos--;
        buf[pos] = digits[value % base];
        value /= base;
    }

    return &buf[pos];
}

static void kprintf_putint(int64_t value) {
    char buf[21]; 
    uint64_t uval;

    if (value < 0) {
        serial_write_char('-');
        uval = (uint64_t)(-value);
    } else {
        uval = (uint64_t)value;
    }

    char *s = uint_to_str(uval, 10, buf, sizeof(buf));
    serial_write_string(s);
}

static void kprintf_puthex(uint64_t value) {
    char buf[17]; 
    char *s = uint_to_str(value, 16, buf, sizeof(buf));
    serial_write_string(s);
}


void kprintf(const char *fmt, ...) {
    uint64_t f = 0;
    if (kprintf_lock_ready) {
        f = spinlock_acquire(&kprintf_lock);
    }

    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            serial_write_char(*p);
            continue;
        }

        p++;

        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
        }

        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                serial_write_string(s);
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                serial_write_char(c);
                break;
            }
            case 'd': {
                int64_t val = is_long ? va_arg(args, int64_t) : va_arg(args, int);
                kprintf_putint(val);
                break;
            }
            case 'x': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                kprintf_puthex(val);
                break;
            }
            case '%': {
                serial_write_char('%');
                break;
            }
            case '\0': {
                serial_write_char('%');
                goto done;
            }
            default: {
                serial_write_char('%');
                serial_write_char(*p);
                break;
            }
        }
    }

done:
    va_end(args);

    if (kprintf_lock_ready) {
        spinlock_release(&kprintf_lock, f);
    }
}
