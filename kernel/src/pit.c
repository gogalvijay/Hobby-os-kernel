#include "pit.h"
#include "io.h"
#include "kprintf.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43

#define PIT_BASE_FREQUENCY 1193182u

#define PIT_CMD_CH0_MODE2 0x34

void pit_init(uint32_t frequency_hz) {
    if (frequency_hz < 19) {
        frequency_hz = 19;      
    }

    uint32_t reload = PIT_BASE_FREQUENCY / frequency_hz;
    if (reload > 65535) {
        reload = 65535;
    }
    if (reload == 0) {
        reload = 1;
    }

    outb(PIT_COMMAND, PIT_CMD_CH0_MODE2);

    outb(PIT_CHANNEL0_DATA, (uint8_t)(reload & 0xFF));         
    outb(PIT_CHANNEL0_DATA, (uint8_t)((reload >> 8) & 0xFF));  

    uint32_t actual_hz = PIT_BASE_FREQUENCY / reload;
    kprintf("pit_init: requested=%ldHz reload=%ld actual=%ldHz\n",
            (int64_t)frequency_hz, (int64_t)reload, (int64_t)actual_hz);
}
