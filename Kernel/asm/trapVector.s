/* asm/trapVector.s */

.section .text

.align 2
.global trap_entry
.global trap_entry_pre
.extern trap_Handler_C_pre
.extern kernel_trap_handler

.equ NeededStack, 288       # 8B * 34 Register

trap_entry_pre:
    # Kann derzeit nur Kernel Interrupts (Also vom Kernel erzeugte) sicher speichern.
    # --- Prolog ---
    addi sp, sp, -NeededStack
    sd ra, 0(sp)
    sd fp, 8(sp)
    addi fp, sp, NeededStack

    # --- Body ---
    # Context Saven (Noch keine Prüfung auf FPU)

    # --- A-Register ---
    sd a0, 16(sp)
    sd a1, 24(sp)
    sd a2, 32(sp)
    sd a3, 40(sp)
    sd a4, 48(sp)
    sd a5, 56(sp)
    sd a6, 64(sp)
    sd a7, 72(sp)

    # --- S-Register ---
    sd s1, 80(sp)       # s0 wird ausgelassen weil es intern auch fp ist und wir diesen im Prolog gesaved haben
    sd s2, 88(sp)
    sd s3, 96(sp)
    sd s4, 104(sp)
    sd s5, 112(sp)
    sd s6, 120(sp)
    sd s7, 128(sp)
    sd s8, 136(sp)
    sd s9, 144(sp)
    sd s10, 152(sp)
    sd s11, 160(sp)

    # --- T-Register ---
    sd t0, 168(sp)
    sd t1, 176(sp)
    sd t2, 184(sp)
    sd t3, 192(sp)
    sd t4, 200(sp)
    sd t5, 208(sp)
    sd t6, 216(sp)
    
    # --- ?-Register ---
    sd gp, 224(sp)
    sd tp, 232(sp)
    # Werden in einem Seperatem Struct gespeichert.
    csrr t0, sepc
    csrr t1, sstatus
    csrr t2, scause
    csrr t3, stval
    sd t0, 240(sp)
    sd t1, 248(sp)
    sd t2, 256(sp)
    sd t3, 264(sp)

    # --- Argumente ---
    add a0, zero, sp
    add a1, sp, 240     # Schaut auf Trap_Information

    call trap_Handler_C_pre

    # --- Restore CSRs --- # Mehr nachschauen wieso, dies ist von bare_testing

    ld t0, 240(sp)
    csrw sepc, t0
    ld t0, 248(sp)
    csrw sstatus, t0

    # --- Restore ---

    # --- A-Register ---
    ld a0, 16(sp)
    ld a1, 24(sp)
    ld a2, 32(sp)
    ld a3, 40(sp)
    ld a4, 48(sp)
    ld a5, 56(sp)
    ld a6, 64(sp)
    ld a7, 72(sp)

    # --- S-Register ---
    ld s1, 80(sp)       # s0 wird ausgelassen weil es intern auch fp ist, dieser wird im Epilog gemacht
    ld s2, 88(sp)
    ld s3, 96(sp)
    ld s4, 104(sp)
    ld s5, 112(sp)
    ld s6, 120(sp)
    ld s7, 128(sp)
    ld s8, 136(sp)
    ld s9, 144(sp)
    ld s10, 152(sp)
    ld s11, 160(sp)

    # --- T-Register ---
    ld t0, 168(sp)
    ld t1, 176(sp)
    ld t2, 184(sp)
    ld t3, 192(sp)
    ld t4, 200(sp)
    ld t5, 208(sp)
    ld t6, 216(sp)
    
    # --- ?-Register ---
    ld gp, 224(sp)
    ld tp, 232(sp)

    # --- Epilog ---
    ld ra, 0(sp)
    ld fp, 8(sp)
    addi sp, sp, NeededStack
    
    sret


# Final Stage
trap_entry:
    # Kernel Stack!

    # Prologue: Platz machen
    addi sp, sp, -NeededStack
    
    # Register retten
    sd ra, 0(sp)
    sd fp, 8(sp)    # fp ist dasselbe wie das Register s0 oder x8
    
    # --- A-Register ---
    sd a0, 16(sp)
    sd a1, 24(sp)
    sd a2, 32(sp)
    sd a3, 40(sp)
    sd a4, 48(sp)
    sd a5, 56(sp)
    sd a6, 64(sp)
    sd a7, 72(sp)

    # --- S-Register ---
    sd s1, 80(sp)       # s0 wird ausgelassen weil es intern auch fp ist und wir diesen im Prolog gesaved haben
    sd s2, 88(sp)
    sd s3, 96(sp)
    sd s4, 104(sp)
    sd s5, 112(sp)
    sd s6, 120(sp)
    sd s7, 128(sp)
    sd s8, 136(sp)
    sd s9, 144(sp)
    sd s10, 152(sp)
    sd s11, 160(sp)

    # --- T-Register ---
    sd t0, 168(sp)
    sd t1, 176(sp)
    sd t2, 184(sp)
    sd t3, 192(sp)
    sd t4, 200(sp)
    sd t5, 208(sp)
    sd t6, 216(sp)

    # --- ?-Register ---
    sd gp, 224(sp)
    sd tp, 232(sp)

    csrr t0, sepc
    csrr t1, sstatus
    csrr t2, scause
    csrr t3, stval
    sd t0, 240(sp)
    sd t1, 248(sp)
    sd t2, 256(sp)
    sd t3, 264(sp)

    add a0, zero, sp
    add a1, sp, 240

    call kernel_trap_handler

    # --- Restore ---

    ld t0, 240(sp)
    csrw sepc, t0
    ld t0, 248(sp)
    csrw sstatus, t0

    # --- A-Register ---
    ld a0, 16(sp)
    ld a1, 24(sp)
    ld a2, 32(sp)
    ld a3, 40(sp)
    ld a4, 48(sp)
    ld a5, 56(sp)
    ld a6, 64(sp)
    ld a7, 72(sp)

    # --- S-Register ---
    ld s1, 80(sp)       # s0 wird ausgelassen weil es intern auch fp ist, dieser wird im Epilog gemacht
    ld s2, 88(sp)
    ld s3, 96(sp)
    ld s4, 104(sp)
    ld s5, 112(sp)
    ld s6, 120(sp)
    ld s7, 128(sp)
    ld s8, 136(sp)
    ld s9, 144(sp)
    ld s10, 152(sp)
    ld s11, 160(sp)

    # --- T-Register ---
    ld t0, 168(sp)
    ld t1, 176(sp)
    ld t2, 184(sp)
    ld t3, 192(sp)
    ld t4, 200(sp)
    ld t5, 208(sp)
    ld t6, 216(sp)
    
    # --- ?-Register ---
    ld gp, 224(sp)
    ld tp, 232(sp)

    # --- Epilog ---
    ld ra, 0(sp)
    ld fp, 8(sp)
    addi sp, sp, NeededStack

    sret


