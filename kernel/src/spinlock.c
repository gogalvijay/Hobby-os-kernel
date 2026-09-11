#include "spinlock.h"

void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

static inline uint64_t save_and_disable_interrupts(void) {
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(rflags));
    return rflags;
}

static inline void restore_interrupts(uint64_t rflags) {
    __asm__ volatile ("push %0; popfq" :: "r"(rflags) : "memory", "cc");
}

uint64_t spinlock_acquire(spinlock_t *lock) {
    uint64_t flags = save_and_disable_interrupts();
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
   }
    return flags;
}

void spinlock_release(spinlock_t *lock, uint64_t flags) {
    __sync_lock_release(&lock->locked);
    restore_interrupts(flags);
}
