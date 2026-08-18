# RISC-V Supervisor-Mode Trap Causes (S-Mode)
Mit Bedeutung von `stval`

## Interrupts (MSB = 1)
Bei Interrupts ist `stval` **immer 0**, da keine fehlerhafte Adresse oder Instruktion beteiligt ist.

| Cause-Code | Name                          | Beschreibung                                      | stval-Inhalt |
|------------|--------------------------------|---------------------------------------------------|--------------|
| 1          | Supervisor Software Interrupt  | Durch Software/Kernel/IPI ausgelöst               | 0            |
| 5          | Supervisor Timer Interrupt     | Timer abgelaufen (CLINT/SBI)                      | 0            |
| 9          | Supervisor External Interrupt  | Hardware-Interrupt (über PLIC)                    | 0            |

> Vollständiger `scause` = (1 << (XLEN-1)) \| Cause

---

## Exceptions (MSB = 0)

| Cause-Code | Name                               | Beschreibung                                                                 | stval-Inhalt                                                                 |
|------------|-------------------------------------|-------------------------------------------------------------------------------|-------------------------------------------------------------------------------|
| 0          | Instruction address misaligned      | PC nicht korrekt aligned                                                     | Die fehlerhafte Instruktionsadresse                                           |
| 1          | Instruction access fault            | Lesezugriff auf Instruktionsspeicher fehlgeschlagen (PMP, Busfehler)         | Die fehlerhafte Instruktionsadresse                                           |
| 2          | Illegal instruction                 | Ungültige oder nicht unterstützte Instruktion                                | Die **Instruktion selbst** (32-Bit / 16-Bit Wert)                             |
| 3          | Breakpoint                          | `ebreak` ausgeführt                                                          | Adresse der `ebreak`-Instruktion                                              |
| 4          | Load address misaligned             | Load-Adresse nicht korrekt aligned                                            | Die fehlerhafte Load-Adresse                                                  |
| 5          | Load access fault                   | Physischer Load fehlgeschlagen (PMP-Fehler, MMIO ungültig, Busfehler)        | Die fehlgeschlagene Load-Adresse                                              |
| 6          | Store/AMO address misaligned        | Store-/AMO-Adresse nicht korrekt aligned                                      | Die fehlerhafte Store-/AMO-Adresse                                            |
| 7          | Store/AMO access fault              | Physischer Store/AMO fehlgeschlagen (PMP, MMIO ungültig, Busfehler)           | Die fehlgeschlagene Store-/AMO-Adresse                                        |
| 8          | Environment call from U-mode        | Syscall aus User-Mode                                                         | 0                                                                             |
| 9          | Environment call from S-mode        | Syscall aus Supervisor-Mode                                                   | 0                                                                             |
| 12         | Instruction page fault              | Virtuelle Instruktionsadresse nicht gemappt oder ohne Rechte                  | Die virtuelle Adresse, auf die zugegriffen wurde                             |
| 13         | Load page fault                     | Virtuelle Load-Adresse nicht gemappt oder ohne Rechte                         | Die virtuelle Adresse, auf die zugegriffen wurde                             |
| 15         | Store/AMO page fault                | Virtuelle Store-/AMO-Adresse nicht gemappt oder ohne Rechte                   | Die virtuelle Adresse, auf die zugegriffen wurde                             |

---

## Kurzfassung: Wann ist `stval` wichtig?

- **Page Faults:**  
  → `stval` enthält **die virtuelle Adresse**, die nicht gemappt oder ohne Rechte war.

- **Access Faults:**  
  → `stval` enthält **die physische oder virtuelle Adresse**, deren Zugriff hardwareseitig fehlgeschlagen ist  
    (PMP blockiert, MMIO ungültig, Busfehler).

- **Misaligned Accesses:**  
  → `stval` ist **immer die fehlerhafte Adresse**, die nicht korrekt aligned war.

- **Illegal Instruction:**  
  → `stval` enthält **den kompletten Instruktionswert**.

- **Syscalls (`ecall`) & Interrupts:**  
  → `stval = 0`.

---

# Wie sollte der Kernel auf Page Faults und Access Faults reagieren?

## 🟦 Page Faults
Page Faults entstehen durch **MMU/Paging** und können harmlos oder gefährlich sein.

### User Mode Page Faults – drei mögliche Kategorien

#### 1. **Legitim (Demand Paging / Lazy Allocation / Code noch nicht im RAM)**
Beispiele:
- Prozess greift auf eine Seite zu, die zwar im Page Table steht, aber noch nicht geladen wurde  
- Heap vergrößert sich (brk/sbrk)  
- Stack wächst nach unten  
- Memory-Mapped Files werden on-demand geladen

→ Kernel lädt Seite nach  
→ Prozess läuft weiter  

*(Hinweis: RISC-V selbst zwingt dich nicht zu Demand Paging, aber OS-Design erlaubt es.)*

#### 2. **Programmierfehler im Userprozess**
Beispiele:
- Null-Pointer  
- ungültiger Pointer  
- Zugriff auf ungemappte Adresse  

→ Prozess beenden  
→ Kernel bleibt stabil  

#### 3. **Bösartiger Zugriff**
Wenn ein Userprozess absichtlich:
- Kernelbereiche mappt  
- MMIO-Adressen mappt  
- schreibend in nicht erlaubte Bereiche greift  

→ Sofort Prozess killen  
→ Optionale Security-Logs  

---

### Kernel Mode Page Faults
→ Fast immer ein **Bug im Kernel**, z. B.:

- Page Table falsch aufgebaut  
- Kernel-Stack nicht gemappt  
- Mapping für Framebuffer oder Kerneltext fehlt  

→ Sofort Panic / fKernel aktivieren

---

## 🟥 Access Faults
Access Faults entstehen **nicht durch Paging**, sondern durch Hardware- oder physische Adressfehler:

- physische Adresse existiert nicht  
- ungültige oder falsche MMIO-Adresse  
- Gerät antwortet nicht (Busfehler)  
- PMP blockiert  
- Kernel schreibt in nicht vorhandenen RAM  

### Kernel Mode:
→ **Sofort Panic**  
→ Loggen (`scause`, `stval`, `sepc`)  
→ fKernel starten  

### User Mode:
→ Höchstwahrscheinlich absichtlich oder grob fehlerhaft  
→ **Prozess killen**

---

# Wichtigster Unterschied

| Fault-Typ | Ursache | Normal im User Mode? | Behandlung | Bedeutung |
|----------|---------|----------------------|------------|-----------|
| **Page Fault** | MMU / Virtual Memory | ✔ Ja (Demand Paging, Lazy Mapping) | Load page / kill user / panic im Kernel | Software-Virtualisierung |
| **Access Fault** | Hardware / physische Adresse / PMP | ❌ Nein | User: kill / Kernel: panic | Schwerer Fehler |

