#include <stdint.h>

#define COM1_PORT 0x3F8

// I/O port helper functions
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Register offsets from COM1_PORT
#define UART_DATA          (COM1_PORT + 0) // DLAB=0: data reg | DLAB=1: divisor low
#define UART_INT_ENABLE    (COM1_PORT + 1) // DLAB=0: IER      | DLAB=1: divisor high
#define UART_FIFO_CTRL     (COM1_PORT + 2)
#define UART_LINE_CTRL     (COM1_PORT + 3)
#define UART_MODEM_CTRL    (COM1_PORT + 4)
#define UART_LINE_STATUS   (COM1_PORT + 5)

#define LCR_DLAB           0x80 // Divisor Latch Access Bit
#define LCR_8N1            0x03 // 8 data bits, no parity, 1 stop bit
#define LSR_TX_EMPTY       0x20 // bit 5: Transmitter Holding Register Empty

void serial_init(void) {

    //Check Notes for the steps to verify it 
    
	
    outb(UART_INT_ENABLE, 0x00);   // disable UART interrupts, we're polling

    // set baud rate divisor (115200 / divisor = actual baud)
    // divisor = 1 -> 115200 baud
    outb(UART_LINE_CTRL, LCR_DLAB);        // enable DLAB to access divisor latch
    outb(UART_DATA, 0x01);                 // divisor low byte
    outb(UART_INT_ENABLE, 0x00);           // divisor high byte

    outb(UART_LINE_CTRL, LCR_8N1);         // 8N1, DLAB back to 0

    outb(UART_FIFO_CTRL, 0xC7);            // enable FIFO, clear it, 14-byte threshold

    outb(UART_MODEM_CTRL, 0x0B);           // enable RTS, DTS, OUT2 (needed for interrupts later)

    // --- loopback self-test ---
    outb(UART_MODEM_CTRL, 0x1E);           // enable loopback mode
    outb(UART_DATA, 0xAE);                 // send test byte
    if (inb(UART_DATA) != 0xAE) {
        // UART is faulty or not present — halt or handle as you see fit
        for (;;) { __asm__ volatile ("hlt"); }
    }

    outb(UART_MODEM_CTRL, 0x0F);           // take back out of loopback, normal operation
}

static int serial_tx_empty(void) {
    return inb(UART_LINE_STATUS) & LSR_TX_EMPTY;
}

void serial_write_char(char c) {
    while (!serial_tx_empty());
    outb(UART_DATA, (uint8_t)c);
}

void serial_write_string(const char *str) {
    while (*str != '\0') {
        if (*str == '\n') {
            serial_write_char('\r'); 
        }
        serial_write_char(*str);
        str++;
    }
}
