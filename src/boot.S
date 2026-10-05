.section .text.boot
.global _start
_start:
    msr daifset, #15
    msr spsel, #1
    mrs x0, mpidr_el1
    and x0, x0, #3
    cbz x0, primary
    b secondary
park:
    wfe
    b park
secondary:
    /* 64 KiB per core, distinct from the primary kernel stack. */
    ldr x1, =__stack_top
    mov x2, #65536
    mul x2, x0, x2
    sub x1, x1, x2
    mov sp, x1
    mov x19, x0
    bl holly_smp_worker
    b park
.global holly_secondary_entry
holly_secondary_entry:
    mrs x0, mpidr_el1
    and x0, x0, #3
    cbnz x0, secondary
    b park
primary:
    ldr x1, =__bss_start
    ldr x2, =__bss_end
zero_bss:
    cmp x1, x2
    b.hs ready
    str xzr, [x1], #8
    b zero_bss
ready:
    ldr x0, =__stack_top
    mov sp, x0
    ldr x1, =holly_vectors
    mrs x0, CurrentEL
    cmp x0, #8
    b.ne vector_el1
    msr vbar_el2, x1
    b vectors_ready
vector_el1:
    msr vbar_el1, x1
vectors_ready:
    isb
    bl kernel_main
halt:
    wfe
    b halt
.section .note.GNU-stack,"",%progbits
