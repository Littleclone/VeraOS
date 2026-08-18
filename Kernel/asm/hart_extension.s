.section .text

.global k_init_test_timer

k_init_test_timer:
    # 1. Timer Interrupt im S-Mode aktivieren (Bit 5 = 0x20)
    li t0, 0x20
    csrs sie, t0

    # 2. Aktuelle Zeit lesen
    csrr t0, time

    # 3. Offset in ein Hilfsregister laden (10.000.000 Ticks = 1 Sekunde)
    li t1, 10000000
    
    # 4. Zeit + Offset berechnen
    add t0, t0, t1

    # 5. Wecker in stimecmp eintragen
    csrw stimecmp, t0
    
    wfi
