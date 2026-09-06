#include "syscall.h"
#include "kprintf.h"
#include "task.h"
#include "uaccess.h"
#include "scheduler.h"



static long sys_fork(struct syscall_regs *regs) {
    if (current_task == NULL) {
        return -1;
    }
    struct task *child = task_fork(current_task, regs);
    if (child == NULL) {
        return -1;
    }
    return (long)child->task_id;
}


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

//static long sys_exit(uint64_t code) {
  //  kprintf("\nuser program exited with code=%ld\n", (long)code);
    //for (;;) {
      //  __asm__ volatile ("cli; hlt");
    //}
    //return 0; // unreachable
//}


static long sys_exit(uint64_t code) {
    kprintf("\nuser program exited with code=%ld\n", (long)code);

    if (current_task != NULL) {
        current_task->state = TASK_DEAD;
    }
    
    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
    return 0;
}
static long sys_send(uint64_t target_id, uint64_t user_buf, uint64_t len) {
    if (current_task == NULL) {
        return -1;
    }
    if (len > 256) {
        len = 256;
    }
    if (!uva_check_range(current_task->pml4, user_buf, len, false)) {
        kprintf("sys_send: invalid source pointer 0x%lx len=0x%lx\n", user_buf, len);
        return -1;
    }

    struct task *target = task_find_by_id(target_id);
    if (target == NULL) {
        kprintf("sys_send: no such task_id=%ld\n", (int64_t)target_id);
        return -1;
    }

    if (target->state == TASK_BLOCKED &&
        target->block_reason == BLOCK_ON_RECV &&
        (target->ipc_peer == IPC_ANY_SENDER || target->ipc_peer == current_task->task_id)) {

        uint64_t copy_len = len < target->ipc_len ? len : target->ipc_len;

        if (!uva_check_range(target->pml4, target->ipc_buf, copy_len, true)) {
            kprintf("sys_send: target's recv buffer no longer valid\n");
            return -1;
        }

        char tmp[256];
        const char *src = (const char *)user_buf;      
        for (uint64_t i = 0; i < copy_len; i++) {
            tmp[i] = src[i];
        }

        page_table_t *sender_pml4 = get_current_pml4();
        vmm_switch_address_space(target->pml4);
        char *dst = (char *)target->ipc_buf;
        for (uint64_t i = 0; i < copy_len; i++) {
            dst[i] = tmp[i];
        }
        vmm_switch_address_space(sender_pml4);

        task_wake(target);
        return (long)copy_len;
    }

    task_block_and_switch(BLOCK_ON_SEND, target_id, user_buf, len);

    return (long)current_task->ipc_len;
}

static long sys_recv(uint64_t from_id, uint64_t user_buf, uint64_t len) {
    if (current_task == NULL) {
        return -1;
    }
    if (len > 256) {
        len = 256;
    }
    if (!uva_check_range(current_task->pml4, user_buf, len, true)) {
        kprintf("sys_recv: invalid destination pointer 0x%lx len=0x%lx\n", user_buf, len);
        return -1;
    }

    for (size_t i = 0; i < task_table_size(); i++) {
        struct task *sender = task_table_ptr(i);
        if (sender == NULL || sender->state != TASK_BLOCKED) {
            continue;
        }
        if (sender->block_reason != BLOCK_ON_SEND) {
            continue;
        }
        if (sender->ipc_peer != current_task->task_id) {
            continue;
        }
        if (from_id != IPC_ANY_SENDER && sender->task_id != from_id) {
            continue;
        }

        uint64_t copy_len = len < sender->ipc_len ? len : sender->ipc_len;

        if (!uva_check_range(sender->pml4, sender->ipc_buf, copy_len, false)) {
            kprintf("sys_recv: sender's buffer no longer valid\n");
            continue;
        }

        page_table_t *receiver_pml4 = get_current_pml4();
        vmm_switch_address_space(sender->pml4);
        char tmp[256];
        const char *src = (const char *)sender->ipc_buf;
        for (uint64_t j = 0; j < copy_len; j++) {
            tmp[j] = src[j];
        }
        vmm_switch_address_space(receiver_pml4);

        char *dst = (char *)user_buf;
        for (uint64_t j = 0; j < copy_len; j++) {
            dst[j] = tmp[j];
        }

        sender->ipc_len = copy_len;  
        task_wake(sender);

        return (long)copy_len;
    }

    task_block_and_switch(BLOCK_ON_RECV, from_id, user_buf, len);

    return (long)current_task->ipc_len;
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
        case SYS_FORK:
            ret = sys_fork(regs);
            break;

        case SYS_SEND:
            ret = sys_send(regs->rdi, regs->rsi, regs->rdx);
            break;
        case SYS_RECV:
            ret = sys_recv(regs->rdi, regs->rsi, regs->rdx);
            break;	    
	default:
            kprintf("syscall_dispatch: unknown syscall %ld\n", (long)regs->rax);
            ret = -1;
            break;
    }

    regs->rax = (uint64_t)ret;
}
