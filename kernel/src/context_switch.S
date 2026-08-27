.section .text
.global context_switch

context_switch:
    test %rdi, %rdi
    jz   .load_new

    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    mov  %rsp, 32(%rdi)

.load_new:
    mov  32(%rsi), %rsp

    pop  %r15
    pop  %r14
    pop  %r13
    pop  %r12
    pop  %rbp
    pop  %rbx

    ret
