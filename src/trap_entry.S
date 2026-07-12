# this is what will be loaded into the stvec CSR; the
# trap handler address must be 4-byte aligned:
# https://docs.riscv.org/reference/isa/priv/supervisor.html#12-1-1-2-supervisor-trap-vector-base-address-stvec-register
#
# Since a trap can interrupt arbitrary code at any point (especially
# asynchronous interrupts), we must save every register we might clobber
# before jumping into the C handler and restore them all before returning
# with sret. We skip x0 (hardwired zero) and x2 (sp), which is restored
# implicitly since we adjust it by a fixed amount.
.globl trap_entry
.align 4
trap_entry:
    addi sp, sp, -256

    sd x1,   0(sp)   # ra
    sd x3,  16(sp)   # gp
    sd x4,  24(sp)   # tp
    sd x5,  32(sp)   # t0
    sd x6,  40(sp)   # t1
    sd x7,  48(sp)   # t2
    sd x8,  56(sp)   # s0/fp
    sd x9,  64(sp)   # s1
    sd x10, 72(sp)   # a0
    sd x11, 80(sp)   # a1
    sd x12, 88(sp)   # a2
    sd x13, 96(sp)   # a3
    sd x14, 104(sp)  # a4
    sd x15, 112(sp)  # a5
    sd x16, 120(sp)  # a6
    sd x17, 128(sp)  # a7
    sd x18, 136(sp)  # s2
    sd x19, 144(sp)  # s3
    sd x20, 152(sp)  # s4
    sd x21, 160(sp)  # s5
    sd x22, 168(sp)  # s6
    sd x23, 176(sp)  # s7
    sd x24, 184(sp)  # s8
    sd x25, 192(sp)  # s9
    sd x26, 200(sp)  # s10
    sd x27, 208(sp)  # s11
    sd x28, 216(sp)  # t3
    sd x29, 224(sp)  # t4
    sd x30, 232(sp)  # t5
    sd x31, 240(sp)  # t6

    call handle_trap

    ld x1,   0(sp)
    ld x3,  16(sp)
    ld x4,  24(sp)
    ld x5,  32(sp)
    ld x6,  40(sp)
    ld x7,  48(sp)
    ld x8,  56(sp)
    ld x9,  64(sp)
    ld x10, 72(sp)
    ld x11, 80(sp)
    ld x12, 88(sp)
    ld x13, 96(sp)
    ld x14, 104(sp)
    ld x15, 112(sp)
    ld x16, 120(sp)
    ld x17, 128(sp)
    ld x18, 136(sp)
    ld x19, 144(sp)
    ld x20, 152(sp)
    ld x21, 160(sp)
    ld x22, 168(sp)
    ld x23, 176(sp)
    ld x24, 184(sp)
    ld x25, 192(sp)
    ld x26, 200(sp)
    ld x27, 208(sp)
    ld x28, 216(sp)
    ld x29, 224(sp)
    ld x30, 232(sp)
    ld x31, 240(sp)

    addi sp, sp, 256

    sret
