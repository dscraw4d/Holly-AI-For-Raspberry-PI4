.section .text
.balign 2048
.global holly_vectors
holly_vectors:
.rept 4
    b holly_vector_entry
    .space 124
.rept 3
    b holly_fatal_vector
    .space 124
.endr
.endr
holly_fatal_vector:
    wfe
    b holly_fatal_vector
holly_vector_entry:
    sub sp, sp, #288
    stp x0,x1,[sp,#0]
    stp x2,x3,[sp,#16]
    stp x4,x5,[sp,#32]
    stp x6,x7,[sp,#48]
    stp x8,x9,[sp,#64]
    stp x10,x11,[sp,#80]
    stp x12,x13,[sp,#96]
    stp x14,x15,[sp,#112]
    stp x16,x17,[sp,#128]
    stp x18,x19,[sp,#144]
    stp x20,x21,[sp,#160]
    stp x22,x23,[sp,#176]
    stp x24,x25,[sp,#192]
    stp x26,x27,[sp,#208]
    stp x28,x29,[sp,#224]
    str x30,[sp,#240]
    mrs x4,CurrentEL
    cmp x4,#8
    b.ne 1f
    mrs x0,elr_el2
    mrs x1,spsr_el2
    mrs x2,esr_el2
    mrs x3,far_el2
    b 2f
1:
    mrs x0,elr_el1
    mrs x1,spsr_el1
    mrs x2,esr_el1
    mrs x3,far_el1
2:
    stp x0,x1,[sp,#248]
    stp x2,x3,[sp,#264]
    str x4,[sp,#280]
    mov x0,sp
    bl holly_exception_dispatch
    ldp x0,x1,[sp,#248]
    ldr x4,[sp,#280]
    cmp x4,#8
    b.ne 3f
    msr elr_el2,x0
    msr spsr_el2,x1
    b 4f
3:
    msr elr_el1,x0
    msr spsr_el1,x1
4:
    ldp x0,x1,[sp,#0]
    ldp x2,x3,[sp,#16]
    ldp x4,x5,[sp,#32]
    ldp x6,x7,[sp,#48]
    ldp x8,x9,[sp,#64]
    ldp x10,x11,[sp,#80]
    ldp x12,x13,[sp,#96]
    ldp x14,x15,[sp,#112]
    ldp x16,x17,[sp,#128]
    ldp x18,x19,[sp,#144]
    ldp x20,x21,[sp,#160]
    ldp x22,x23,[sp,#176]
    ldp x24,x25,[sp,#192]
    ldp x26,x27,[sp,#208]
    ldp x28,x29,[sp,#224]
    ldr x30,[sp,#240]
    add sp,sp,#288
    eret
.global holly_cpu_probe32
.type holly_cpu_probe32,%function
holly_cpu_probe32:
    tst x0,#3
    b.ne holly_probe_fail
.global holly_probe_load
holly_probe_load:
    ldr w2,[x0]
    str w2,[x1]
    mov w0,#0
    ret
.global holly_probe_fail
holly_probe_fail:
    mov w0,#-1
    ret
.section .note.GNU-stack,"",%progbits
