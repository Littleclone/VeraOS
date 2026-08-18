/* asm/start.s */

.section .text
.global _start
.global kernel_jump
.extern lily_main  // temp
.extern k_main
.extern trap_entry_lily
.extern trap_entry_pre

_start:
    # Stack setzen
    lla sp, _stack_top              # Setzen den Stack Pointer zum Anfang des Temporären Stacks
    addi sp, sp, -144               # Setze den Stack runter, so das wir Platz haben, gespeichert wird: Die Kernel Area sowie den Return Pointer

    # .bss sauber auf 0 setzen
    lla t0, __bss_start
    lla t1, __bss_end

.CleanLoop:                 # Hier wird der gesammte .bss auf 0 gesetzt
    beq t0, t1, .LilyInit
    sd zero, 0(t0)
    addi t0, t0, 8
    j .CleanLoop

.LilyInit:
    # Temp Interrupt Init
    lla t0, trap_entry_lily   # Lade die Adresse
    csrw stvec, t0       # stvec = &trap_entry (direct mode)

    csrr t0, sstatus        # Read sstatus
    ori t0, t0, 1 << 1      # SIE setzen (1 zu bit 1 schieben (Bits fangen mit 0 an))
    csrw sstatus, t0        # Schreib zurück

    # Hole die kernel bereiche und speichere sie auf dem Stack
    lla t0, __kernel_start
    sd t0, 0(sp)
    lla t0, __kernel_end
    sd t0, 8(sp)
    lla t0, __text
    sd t0, 16(sp)
    lla t0, __text_end
    sd t0, 24(sp)
    lla t0, __rodata
    sd t0, 32(sp)
    lla t0, __rodata_end
    sd t0, 40(sp)
    lla t0, __data
    sd t0, 48(sp)
    lla t0, __data_end
    sd t0, 56(sp)
    lla t0, __sdata
    sd t0, 64(sp)
    lla t0, __sdata_end
    sd t0, 72(sp)
    lla t0, __bss_start
    sd t0, 80(sp)
    lla t0, __bss_end
    sd t0, 88(sp)
    lla t0, __stack_guard_start
    sd t0, 96(sp)
    lla t0, __stack_guard_end
    sd t0, 104(sp)
    lla t0, __stack_start
    sd t0, 112(sp)
    lla t0, __stack_end
    sd t0, 120(sp)
    add t0, zero, zero
    sd t0, 128(sp)  # Nimmt bytes 128 - 135, ra nimmt 136 - 144

    # Setzen in a2 (Drittes argument) den pointer wo gerade sp zeigt, 
    add a2, zero, sp
    jal ra, lily_main

halt:
    wfi
    j halt


# a0 -> Kernel_Boot_info Pointer, a1 hard_ID bootet
kernel_jump:
    # Stack setzen
    lla sp, _stack_top              # Setzen den Stack Pointer zum Anfang des Temporären Stacks

    # .bss sauber auf 0 setzen
    lla t0, __bss_start
    lla t1, __bss_end

.CleanLoop_2:                 # Hier wird der gesammte .bss auf 0 gesetzt
    beq t0, t1, .KernelInit
    sd zero, 0(t0)
    addi t0, t0, 8
    j .CleanLoop_2

.KernelInit:
    lla t0, trap_entry_pre   # Lade die Adresse
    csrw stvec, t0       # stvec = &trap_entry (direct mode)

    csrr t0, sstatus        # Read sstatus
    ori t0, t0, 1 << 1      # SIE setzen (1 zu bit 1 schieben (Bits fangen mit 0 an))
    csrw sstatus, t0        # Schreib zurück

    # Stack
    add fp, zero, sp
    addi sp, sp, -16

    sd fp, 0(sp)

    jal ra, k_main
halt_2:
    wfi
    j halt_2

.section .stack_guard
.align 16
_stack_guard:
    .space 4096
_stack_guard_end:

.section .stack
.align 16

_stack:
    .space 32768   # 32 KiB reserviert
_stack_top: