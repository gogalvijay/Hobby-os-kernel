#include "usermode.h"

void enter_usermode(uint64_t entry, uint64_t user_stack_top) {
    uint64_t user_ss     = USER_DS;
    uint64_t user_cs     = USER_CS;
    uint64_t user_rflags = 0x2; // bit 1 always set, bit 9 (IF) = interrupts enabled

    __asm__ volatile (
        "mov %w[user_ss], %%ds\n"
        "mov %w[user_ss], %%es\n"
        "mov %w[user_ss], %%fs\n"
        "mov %w[user_ss], %%gs\n"

        // build the frame iretq expects, pushed in reverse of pop order
        "push %[user_ss]\n"     // SS
        "push %[user_rsp]\n"    // RSP  (user stack)
        "push %[user_rflags]\n" // RFLAGS
        "push %[user_cs]\n"     // CS   (bottom 2 bits = 3 -> this is what drops CPL)
        "push %[entry]\n"       // RIP  (ends up on top, popped first)
        "iretq\n"
        :
        : [user_ss] "r" (user_ss),
          [user_cs] "r" (user_cs),
          [user_rsp] "r" (user_stack_top),
          [user_rflags] "r" (user_rflags),
          [entry] "r" (entry)
        : "memory"
    );
}
