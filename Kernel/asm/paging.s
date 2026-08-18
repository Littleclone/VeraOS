.section .text

.global activatePaging
.global switchPageTable
.global flushTLB_Cache


    # Erwartet a0 -> pointer zu Root-Table, a1 -> asid
activatePaging:
    li t0, 8
    sll t0, t0, 60
    srli a0, a0, 12
    slli a1, a1, 44      # zu ASID
    or t0, t0, a0
    or t0, t0, a1

    csrw satp, t0       # Paging Aktiv
    fence rw, rw
    sfence.vma   # TLB flushen
    fence rw, rw
    ret

    # Erwartet a0 -> pointer zu Root-Table, a1 -> asid
switchPageTable:
    li t0, 8
    sll t0, t0, 60
    srli a0, a0, 12
    slli a1, a1, 44      # zu ASID
    or t0, t0, a0
    or t0, t0, a1

    csrw satp, t0       # Paging Aktiv
    ret

flushTLB_Cache:
    fence rw, rw
    sfence.vma
    fence rw, rw
    ret
