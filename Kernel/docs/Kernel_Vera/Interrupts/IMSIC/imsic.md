# 📨 RISC-V IMSIC (Incoming Message Signaled Interrupt Controller)

Diese Dokumentation beschreibt die Funktionsweise und Konfiguration des IMSIC, welcher im RISC-V AIA (Advanced Interrupt Architecture) die Message Signaled Interrupts (MSIs) für die CPU entgegennimmt.

## 🤝 1. Die Rolle des IMSIC (APLIC vs. IMSIC)
*   **APLIC:** Sammelt Signale von Kabeln (Wires) und wandelt sie in digitale Nachrichten (MSIs) um.
*   **IMSIC:** Sitzt direkt an jedem CPU-Kern (Hart). Er empfängt diese MSIs, sortiert sie nach Priorität und unterbricht die CPU.

## 🌗 2. Die zwei Gesichter des IMSIC (MMIO vs. CSR)
Der IMSIC hat eine architektonische Besonderheit: Er wird auf zwei völlig verschiedene Arten angesprochen, je nachdem, *wer* mit ihm redet.

1.  **Für Sender (Geräte / APLIC):** Memory-Mapped I/O (MMIO). Jeder IMSIC hat eine physische Speicheradresse (eine 4KB "Message File" Page). Wenn ein Gerät einen Interrupt auslösen will, schreibt es einfach die ID (EIID) in diese Speicheradresse.
2.  **Für die CPU (Unser Kernel-Code):** Control and Status Registers (CSRs). Die CPU nutzt spezielle, extrem schnelle interne Register, um den IMSIC zu konfigurieren und Interrupts abzuarbeiten.

## 🎛️ 3. Wichtige CSRs (Supervisor Mode)
Um mit dem IMSIC zu interagieren, nutzen wir im C-Code Inline-Assembly (z.B. `csrr`, `csrw`). 

### Indirekter Zugriff (`siselect` & `sireg`)
Der IMSIC hat intern viele Konfigurationsregister, aber die CPU hat nicht für jedes einen eigenen Befehl. Stattdessen wird ein "Fenster" genutzt:
*   `siselect` (Supervisor Interrupt Select): Hier schreiben wir rein, *welches* interne IMSIC-Register wir bearbeiten wollen (z.B. `eidelivery`).
*   `sireg` (Supervisor Interrupt Register): Über dieses Register lesen oder schreiben wir dann den tatsächlichen Wert.

### Die drei wichtigsten internen IMSIC-Zustände:
1.  **`eidelivery` (External Interrupt Delivery):** Der Hauptschalter. Muss auf `1` gesetzt werden, damit der IMSIC überhaupt Signale an die CPU weiterreicht.
2.  **`eithreshold` (External Interrupt Threshold):** Bestimmt die Mindestpriorität. Eine `0` bedeutet: Alle Interrupts werden durchgelassen.
3.  **`stopei` (Supervisor Top External Interrupt):** Das wichtigste Register für den Interrupt-Handler! 
    *   **Lesen:** Verrät der CPU die ID (EIID) des aktuell wichtigsten anstehenden Interrupts.
    *   **Schreiben (Claiming):** Wenn wir in dieses Register schreiben, bestätigen wir dem IMSIC: *"Danke, ich habe den Interrupt gesehen und bearbeite ihn jetzt."*

## 🔄 4. Der Lebenszyklus eines Interrupts
1.  Gerät zieht einen Pin am APLIC auf High.
2.  APLIC formatiert eine MSI und schreibt die EIID in die MMIO-Adresse des IMSIC.
3.  IMSIC prüft `eidelivery` und `eithreshold`. Alles okay? Er unterbricht die CPU.
4.  Unser Kernel springt in den Trap-Handler.
5.  Der Kernel liest `stopei`, um herauszufinden, wer den Interrupt ausgelöst hat (z.B. UART).
6.  Der Kernel bedient das Gerät und schreibt danach zurück in `stopei`, um den Interrupt abzuschließen.