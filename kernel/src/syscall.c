#include "syscall.h"
#include "kprintf.h"
#include "task.h"
#include "uaccess.h"

static long sys_write(uint64_t user_ptr, uint64_t len) {
    if (len > 255) {
        len = 255;
    }

    if (current_task == NULL) {
        kprintf("sys_write: no current task, rejecting\n");
        return -1;
    }

    if (!uva_check_range(current_task->pml4, user_ptr, len, false)) {
        kprintf("sys_write: invalid user pointer 0x%lx len=0x%lx, rejecting\n",
                user_ptr, len);
        return -1;
    }

    char buf[256];
    const char *src = (const char *)user_ptr;
    uint64_t i;
    for (i = 0; i < len; i++) {
        buf[i] = src[i];
    }
    buf[i] = '\0';

    kprintf("%s", buf);
    return (long)len;
}

static long sys_exit(uint64_t code) {
    kprintf("\nuser program exited with code=%ld\n", (long)code);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
    return 0; // unreachable
}

void syscall_dispatch(struct syscall_regs *regs) {
    long ret;

    switch (regs->rax) {
        case SYS_WRITE:
            ret = sys_write(regs->rdi, regs->rsi);
            break;
        case SYS_EXIT:
            ret = sys_exit(regs->rdi);
            break;
        default:
            kprintf("syscall_dispatch: unknown syscall %ld\n", (long)regs->rax);
            ret = -1;
            break;
    }

    regs->rax = (uint64_t)ret;
}
