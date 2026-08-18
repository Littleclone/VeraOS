.section .text

.global start_imsic
.global set_indirect_csr
.global get_interrupt_stopei

start_imsic:
    add t1, zero, ra
    addi t0, zero, 0x200
    csrs sie, t0
    addi a0, zero, 0x70
    addi a1, zero, 0x1
    call set_indirect_csr
    addi a0, zero, 0xC0
    addi a1, zero, 0x2
    call set_indirect_csr
    add ra, zero, t1
    ret
    


# a0 Index, a1 wert zum setzen
set_indirect_csr:
    csrw siselect, a0
    csrw sireg, a1
    ret

# return a0, the Interrupt ID
get_interrupt_stopei:
    add a0, zero, zero
    csrr t0, stopei
    beq t0, zero, .return_stopei

    csrw stopei, t0
    add a0, zero, t0

.return_stopei:
    ret

