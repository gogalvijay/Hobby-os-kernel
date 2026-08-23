#include "idt.h"
#include "kprintf.h"

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idtr idtr;

struct interrupt_frame {
    uint64_t ip;
    uint64_t cs;
    uint64_t flags;
    uint64_t sp;
    uint64_t ss;
};

extern void syscall_entry_asm(void);

static const char *exception_name(uint8_t vector) {
    static const char *names[32] = {
        "Divide Error", "Debug", "NMI", "Breakpoint",
        "Overflow", "BOUND Range Exceeded", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
        "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
        "x87 FPU Error", "Alignment Check", "Machine Check", "SIMD FP Exception",
        "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
    };
    return names[vector];
}

static void fault_handler(uint8_t vector, uint64_t error_code, struct interrupt_frame *frame) {
    int from_user = (frame->cs & 0x3) == 0x3;

    if (from_user) {
        kprintf("\n--- USER PROCESS FAULT: %s (vector %d) ---\n", exception_name(vector), vector);
        kprintf("error_code=0x%lx\n", error_code);
        kprintf("rip=0x%lx cs=0x%lx rflags=0x%lx\n", frame->ip, frame->cs, frame->flags);
        kprintf("rsp=0x%lx ss=0x%lx\n", frame->sp, frame->ss);
        kprintf("Process terminated (this is expected containment, not a kernel bug).\n");
    } else {
        kprintf("\n--- KERNEL EXCEPTION %d: %s ---\n", vector, exception_name(vector));
        kprintf("error_code=0x%lx\n", error_code);
        kprintf("rip=0x%lx cs=0x%lx rflags=0x%lx\n", frame->ip, frame->cs, frame->flags);
        kprintf("rsp=0x%lx ss=0x%lx\n", frame->sp, frame->ss);
        kprintf("System halted.\n");
    }

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

#define DEFINE_ISR_NOERR(n) \
    __attribute__((interrupt)) \
    static void isr##n(struct interrupt_frame *frame) { \
        fault_handler(n, 0, frame); \
    }

#define DEFINE_ISR_ERR(n) \
    __attribute__((interrupt)) \
    static void isr##n(struct interrupt_frame *frame, uint64_t error_code) { \
        fault_handler(n, error_code, frame); \
    }

DEFINE_ISR_NOERR(0)
DEFINE_ISR_NOERR(1)
DEFINE_ISR_NOERR(2)
DEFINE_ISR_NOERR(3)
DEFINE_ISR_NOERR(4)
DEFINE_ISR_NOERR(5)
DEFINE_ISR_NOERR(6)
DEFINE_ISR_NOERR(7)
DEFINE_ISR_ERR(8)
DEFINE_ISR_NOERR(9)
DEFINE_ISR_ERR(10)
DEFINE_ISR_ERR(11)
DEFINE_ISR_ERR(12)
DEFINE_ISR_ERR(13)
DEFINE_ISR_ERR(14)
DEFINE_ISR_NOERR(15)
DEFINE_ISR_NOERR(16)
DEFINE_ISR_ERR(17)
DEFINE_ISR_NOERR(18)
DEFINE_ISR_NOERR(19)
DEFINE_ISR_NOERR(20)
DEFINE_ISR_ERR(21)
DEFINE_ISR_NOERR(22)
DEFINE_ISR_NOERR(23)
DEFINE_ISR_NOERR(24)
DEFINE_ISR_NOERR(25)
DEFINE_ISR_NOERR(26)
DEFINE_ISR_NOERR(27)
DEFINE_ISR_NOERR(28)
DEFINE_ISR_ERR(29)
DEFINE_ISR_ERR(30)
DEFINE_ISR_NOERR(31)

void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type_attr) {
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].selector    = selector;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].reserved    = 0;
}

#define IDT_INTERRUPT_GATE 0x8E
#define IDT_INTERRUPT_GATE_USER 0xEE
#define SYSCALL_VECTOR_LOCAL 0x80

static inline uint16_t get_cs(void) {
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs;
}


void idt_init(void) {

    uint16_t KERNEL_CS = get_cs();

    idt_set_gate(0,  (uint64_t)isr0,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(1,  (uint64_t)isr1,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(2,  (uint64_t)isr2,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(3,  (uint64_t)isr3,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(4,  (uint64_t)isr4,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(5,  (uint64_t)isr5,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(6,  (uint64_t)isr6,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(7,  (uint64_t)isr7,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(8,  (uint64_t)isr8,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(9,  (uint64_t)isr9,  KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(10, (uint64_t)isr10, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(11, (uint64_t)isr11, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(12, (uint64_t)isr12, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(13, (uint64_t)isr13, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(14, (uint64_t)isr14, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(15, (uint64_t)isr15, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(16, (uint64_t)isr16, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(17, (uint64_t)isr17, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(18, (uint64_t)isr18, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(19, (uint64_t)isr19, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(20, (uint64_t)isr20, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(21, (uint64_t)isr21, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(22, (uint64_t)isr22, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(23, (uint64_t)isr23, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(24, (uint64_t)isr24, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(25, (uint64_t)isr25, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(26, (uint64_t)isr26, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(27, (uint64_t)isr27, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(28, (uint64_t)isr28, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(29, (uint64_t)isr29, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(30, (uint64_t)isr30, KERNEL_CS, IDT_INTERRUPT_GATE);
    idt_set_gate(31, (uint64_t)isr31, KERNEL_CS, IDT_INTERRUPT_GATE);

    idt_set_gate(SYSCALL_VECTOR_LOCAL, (uint64_t)syscall_entry_asm, KERNEL_CS, IDT_INTERRUPT_GATE_USER);

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" :: "m"(idtr));
}


static uint64_t idt_entry_offset(struct idt_entry *e) {
    return (uint64_t)e->offset_low
         | ((uint64_t)e->offset_mid  << 16)
         | ((uint64_t)e->offset_high << 32);
}

static void idt_entry_print(uint32_t index, struct idt_entry *e) {
    uint64_t offset  = idt_entry_offset(e);
    uint8_t  present = (e->type_attr >> 7) & 0x1;
    uint8_t  dpl     = (e->type_attr >> 5) & 0x3;
    uint8_t  gate    = e->type_attr & 0xF;

    kprintf("[%d] offset=0x%lx sel=0x%x ist=%d present=%d dpl=%d gate=0x%x\n",
            index, offset, e->selector, e->ist, present, dpl, gate);
}

void idt_dump(void) {
    kprintf("IDT base=0x%lx limit=%d entries=%d\n",
            idtr.base, idtr.limit, IDT_ENTRIES);

    for (uint32_t i = 0; i < 32; i++) {
        idt_entry_print(i, &idt[i]);
    }
    idt_entry_print(0x80, &idt[0x80]);
}
