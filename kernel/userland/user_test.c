static inline long syscall2(long num, long a1, long a2) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static void write_str(const char *s, unsigned long len) {
    syscall2(0 /* SYS_WRITE */, (long)s, (long)len);
}

void _start(void) {
    // --- Test 1: normal syscall path works ---
    const char t1[] = "[test1] normal write syscall: ";
    write_str(t1, sizeof(t1) - 1);
    const char t1_msg[] = "hello from ring 3\n";
    write_str(t1_msg, sizeof(t1_msg) - 1);

    // --- Test 2: kernel pointer must be rejected, and the syscall must
    //             return an error (not crash, not silently succeed) ---
    const char t2[] = "[test2] rejected kernel pointer: ";
    write_str(t2, sizeof(t2) - 1);

    long bad_ret = syscall2(0 /* SYS_WRITE */, 0xffffffff80001860, 16);

    if (bad_ret == -1) {
        const char pass[] = "PASS (syscall correctly returned -1)\n";
        write_str(pass, sizeof(pass) - 1);
    } else {
        const char fail[] = "FAIL (syscall did not reject bad pointer!)\n";
        write_str(fail, sizeof(fail) - 1);
    }

    // --- Test 3: intentional fault must be contained, not a kernel panic.
    //             Check the kernel's serial output for
    //             "--- USER PROCESS FAULT ---" (not "KERNEL EXCEPTION")
    //             and "Process terminated" after this point. ---
    const char t3[] = "[test3] triggering intentional NULL write now...\n";
    write_str(t3, sizeof(t3) - 1);

    volatile int *p = (volatile int *)0;
    *p = 0xDEADBEEF; // expected: USER PROCESS FAULT, cs=0x23, clean containment

    // unreachable if test 3 faults as expected
    const char unreachable[] = "[test3] FAIL: reached code after NULL write!\n";
    write_str(unreachable, sizeof(unreachable) - 1);

    syscall2(1 /* SYS_EXIT */, 0, 0);
    for (;;) { }
}
