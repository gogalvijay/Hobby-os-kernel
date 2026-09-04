#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYSCALL_VECTOR 0x80

#define SYS_WRITE 0
#define SYS_EXIT  1
#define SYS_FORK  2


// Layout must exactly match the push order in syscall_entry.S.
// Fields are listed low-address-first, i.e. in the order they end up
// on the stack after all pushes (last thing pushed = lowest address = offset 0).
struct syscall_regs {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    // hardware-pushed interrupt frame (ring3 -> ring0, so this is the full 5-word form)
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t user_ss;
} __attribute__((packed));

// called from syscall_entry.S with a pointer to the saved register block
void syscall_dispatch(struct syscall_regs *regs);

#endif
