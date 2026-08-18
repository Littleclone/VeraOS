# 🛡️ RISC-V Trap-Handling und AIA (IMSIC) Grundlagen

## 1. Der Trap-Prolog (Der Schutzschild)
Wenn ein Interrupt oder eine Exception auftritt, unterbricht die CPU das aktuelle Programm. Um dessen Zustand nicht zu zerstören, müssen wir ihn sichern.
*   **Stack-Speicher reservieren:** Wir machen Platz auf dem Stack (z. B. 256 Bytes), indem wir den Stack-Pointer nach unten verschieben.
*   **Allgemeine Register retten:** Alle 31 allgemeinen Arbeitsregister (`ra`, `sp`, `a0-a7`, `t0-t6`, `s0-s11` etc.) werden in diesen reservierten Speicherbereich geschrieben.
*   **Spezial-Register (`sepc`) retten:** Das Register `sepc` enthält die genaue Adresse, an der das Programm unterbrochen wurde. Da wir es nicht direkt auf den Stack schreiben können, lesen wir es zuerst in ein allgemeines Register und speichern dieses dann ab.

## 2. Die Brücke zu C
Sobald der Zustand des alten Programms sicher im Speicher liegt, ist unsere Umgebung stabil. Wir können nun gefahrlos in eine in C geschriebene High-Level-Funktion (den Kernel-Trap-Handler) springen. Das ursprüngliche Rücksprungregister (`ra`) ist bereits gesichert, sodass C-Funktionen es überschreiben dürfen.

## 3. Interrupts verwalten mit dem IMSIC
Wenn der C-Handler feststellt, dass es sich um einen externen Hardware-Interrupt handelt (Supervisor External Interrupt), kommunizieren wir mit dem IMSIC der Advanced Interrupt Architecture (AIA).
*   **Das `stopei`-Register:** Dieses CSR (Control and Status Register) ist das Herzstück für externe Interrupts im S-Mode. 
*   **Interrupt Claiming:** Durch das Lesen von `stopei` fragen wir das System nach dem wichtigsten anstehenden Interrupt. Wenn ein Interrupt vorliegt, schreiben wir exakt denselben Wert in das Register zurück. Das bestätigt der Hardware: "Ich kümmere mich jetzt darum, du kannst ihn aus der Liste streichen."
*   **Interrupt-ID extrahieren:** Die eigentliche ID des Geräts (z.B. UART oder Timer) versteckt sich in den oberen Bits des gelesenen Wertes. Durch eine bitweise Rechtsverschiebung (um 16 Bits) holen wir die ID nach ganz unten, um sie als normale Zahl (1, 2, 3...) in C weiterzuverwenden.
*   **Routing:** Anhand dieser ID entscheidet der Kernel, welcher spezifische Gerätetreiber aufgerufen werden muss.

## 4. Der Trap-Epilog (Die Rückkehr)
Nachdem der C-Handler den Interrupt bearbeitet hat, kehren wir in den Assembly-Code zurück, um das unterbrochene Programm exakt so wiederherzustellen, wie wir es verlassen haben.
*   **Register wiederherstellen:** Wir laden die gesicherten Werte vom Stack zurück in die allgemeinen CPU-Register und stellen das `sepc`-Register wieder her.
*   **Stack aufräumen:** Wir schieben den Stack-Pointer wieder um den zuvor reservierten Wert nach oben.
*   **Der Rücksprung:** Mit einem speziellen Supervisor-Return-Befehl springt die CPU zur Adresse aus dem `sepc`-Register und stellt den vorherigen Privilegien-Modus wieder her. Das unterbrochene Programm läuft weiter, als wäre nie etwas passiert.