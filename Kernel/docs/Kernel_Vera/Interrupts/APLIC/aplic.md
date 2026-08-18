# 🛠️ RISC-V APLIC (Advanced Platform Level Interrupt Controller)

Diese Dokumentation beschreibt die Initialisierung und das Routing von externen Interrupts im MSI-Modus (Message Signaled Interrupts) für einen RISC-V S-Mode Kernel.

## 🌳 1. Grundlagen & Device Tree (DTS)
Der APLIC sammelt Signale von externen Geräten (z.B. UART) und leitet sie an den richtigen CPU-Kern (Hart) weiter. 
Informationen über die Geräte kommen dynamisch aus dem Device Tree:
*   `riscv,num-sources`: Gibt an, wie viele Interrupt-Quellen der Controller insgesamt unterstützt. Dies ist unser Limit für die Initialisierungs-Schleife.
*   `interrupts = <10 4>`: Bei einem Peripherie-Gerät gibt die erste Zahl die **Source ID** (den Pin) an und die zweite den **Trigger Mode** (z.B. Level-High).

*Hinweis: Source ID 0 ist architektonisch reserviert und bedeutet "kein Interrupt".*

## 🧠 2. MMIO & Die C-Pointer-Falle
Der APLIC wird über Memory-Mapped I/O (MMIO) gesteuert. Alle Register-Zugriffe **müssen** `volatile` sein, damit der Compiler sie nicht wegoptimiert.

**Wichtigste Regel für Offsets:** 
Die RISC-V Spezifikation gibt Byte-Offsets an (z.B. `0x3000`). Wenn die Basisadresse im C-Code aber ein `uint32_t*` (4 Byte groß) ist, skaliert C jede Addition automatisch um den Faktor 4.
*   Falsch: `base_address + 0x3000` (springt 12.288 Bytes zu weit).
*   Korrekt: `base_address + (0x3000 / 4)` (springt exakt 0x3000 Bytes).

## 🚦 3. `sourcecfg` (Source Configuration)
Bestimmt, *wie* ein Interrupt ausgelöst wird (Trigger Mode, z.B. Wert `6` für Level-High).
*   **Offset:** `0x0000`
*   **Zugriff:** Direkt über den Array-Index der Source ID.
*   **Deaktivierung:** Ein Wert von `0` schaltet die Quelle komplett ab. Wird beim Booten für alle Quellen genutzt, um einen sauberen Zustand herzustellen.

## 🎯 4. `target` (Routing im MSI-Modus)
Bestimmt, *wohin* das Signal geht (Hart ID) und *welche* ID (EIID) die CPU sieht.
*   **Offset:** `0x3000` (in C: `0x3000 / 4`)
*   **Layout (32-Bit):**
    *   Bits 0-10: **EIID** (Software-ID zur Identifikation in der CPU). Geschützt durch `& 0x7FF`.
    *   Bits 18-31: **Hart ID** (Ziel-CPU-Kern). Verschoben durch `<< 18`.

## 🔌 5. `setie` (Set Interrupt Enable)
Standardmäßig blockiert der APLIC alle konfigurierten Signale. Sie müssen explizit scharfgeschaltet ("unmasked") werden.
*   **Offset:** `0x1C00` (in C: `0x1C00 / 4`)
*   **Struktur:** Dies ist kein Array von Registern pro Quelle, sondern eine **Bitmap**. Jedes 32-Bit-Register kontrolliert 32 Quellen (1 Bit pro Quelle).
*   **Berechnung:**
    *   Welches Register? `Index = Source ID / 32`
    *   Welches Bit im Register? `Bit = Source ID % 32`

---

## 💻 Komplette Registrierungs-Funktion

```c
#define Source_Config_Register 0x0004
#define Set_Interrupt_Enable_Register 0x1C00
#define Target_Register 0x3000

#define APLIC_to_Hart(hart_id) (hart_id << 18)
#define APLIC_to_EIID(eiid) (eiid & 0x7FF)
#define APLIC_Target_Register_Format(hart_id, eiid) (APLIC_to_Hart(hart_id) | APLIC_to_EIID(eiid))

void k_aplic_register_interrupt(uint64_t hart_id, uint16_t eiid, uint16_t interrupt_pin, uint8_t mode) {
    if (interrupt_pin == 0) {
        return; // Quelle 0 ist reserviert
    }
    
    volatile uint32_t* base_address = (volatile uint32_t*)P_APLIC_base->base_address;
    
    // 1. Trigger Mode setzen (sourcecfg)
    base_address[interrupt_pin] = mode;
    
    // 2. Routing einstellen (target)
    (base_address + Target_Register / 4)[interrupt_pin] = APLIC_Target_Register_Format(hart_id, eiid);

    // 3. Interrupt scharfschalten (setie Bitmap)
    volatile uint32_t* interrupt_enable_address = base_address + Set_Interrupt_Enable_Register / 4;
    uint16_t register_index = interrupt_pin / 32;
    uint16_t bit_index = interrupt_pin % 32;
    
    interrupt_enable_address[register_index] = (1 << bit_index);
}
