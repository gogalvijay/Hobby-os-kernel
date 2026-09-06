#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "paging.h"
#include "syscall.h"


#define MAX_TASKS 64
#define TASK_KSTACK_PAGES 2
#define TASK_KSTACK_SIZE (TASK_KSTACK_PAGES * 4096)

enum task_state {
    TASK_UNUSED,
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD,
};

enum block_reason {
    BLOCK_NONE,
    BLOCK_ON_RECV,   
    BLOCK_ON_SEND,   
};

#define IPC_ANY_SENDER 0xFFFFFFFFFFFFFFFFULL

struct task {
    uint64_t task_id;
    page_table_t *pml4;
    enum task_state state;
    struct task *next_free;   

    
    uint64_t context_rsp;     
    uint8_t *kstack;         
    uint8_t *kstack_top;      

    // day 50-51: IPC bookkeeping
    enum block_reason block_reason;
    uint64_t ipc_peer;     
                            
    uint64_t ipc_buf;       
    uint64_t ipc_len;       
};

void task_table_init(void);
struct task *task_create(void);
void task_destroy(struct task *t);
void task_dump(void);

// day 40-42
struct task *task_create_kernel(void (*entry)(void));

extern struct task *current_task;

struct task *task_table_ptr(size_t index);
size_t task_table_size(void);

struct task *task_fork(struct task *parent, struct syscall_regs *parent_regs);

// day 50-51
struct task *task_find_by_id(uint64_t task_id);
void task_block_and_switch(enum block_reason reason, uint64_t peer, uint64_t buf, uint64_t len);
void task_wake(struct task *t);


void task_prepare_user_entry(struct task *t, uint64_t entry, uint64_t user_stack_top);

#endif
