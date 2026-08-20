.section .text
.global syscall_entry_asm
.extern syscall_dispatch

# selectors, from gdt.c layout: entry1=kernel code(0x08), entry2=kernel data(0x10)
.equ KERNEL_CS, 0x08
.equ KERNEL_DS, 0x10
# from usermode.h: entry3=user data|3(0x1B), entry4=user code|3(0x23)
.equ USER_DS,   0x1B

syscall_entry_asm:
    # At this point hardware has already, automatically:
    #  - switched CS to KERNEL_CS (from this gate's selector field)
    #  - switched SS:RSP to the kernel stack via TSS.RSP0
    #  - pushed SS, RSP, RFLAGS, CS, RIP (5 qwords) for the ring3->ring0 crossing
    # DS/ES/FS/GS are untouched -- still holding ring-3 values.

    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax
    # 5 hardware pushes + 15 of ours = 20 qwords = 160 bytes = multiple of 16,
    # so if TSS.RSP0 started 16-aligned, RSP is still 16-aligned right here --
    # exactly what the SysV ABI requires immediately before a `call`.

    mov $KERNEL_DS, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov %rsp, %rdi          # arg0 to syscall_dispatch: pointer to the saved regs
    call syscall_dispatch

    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    mov $USER_DS, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    iretq
