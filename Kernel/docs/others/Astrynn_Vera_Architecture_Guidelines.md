# Astrynn OS: Architecture & Vera Kernel Coding Guidelines

> **Status:** Active Reference Document
> **Core Architecture:** RISC-V (Native, From Scratch)
> **Paradigm:** Zero-Trust Monolithic Kernel

Dieses Dokument definiert die Architektur-Philosophie, die Struktur und die C-Coding-Standards für das Betriebssystem **Astrynn** und seinen Kern **Vera**. Es ersetzt veraltete Namenskonzepte und fokussiert sich auf maximale Klarheit, Performance und kompromisslose Sicherheit.

---

## 1. Die System-Trinität (Architektur-Philosophie)

Das System ist in drei logische und thematische Ebenen unterteilt, die klare Zuständigkeiten haben.

### 1.1 Astrynn (Das Betriebssystem / User Space)
*   **Charakter:** Elegant, nutzerzentriert, respektvoll gegenüber der Hardware.
*   **Aufgabe:** Das grafische und funktionale Erlebnis des Nutzers. Es vertraut auf die Sicherheit der tieferen Ebenen und bietet ein reibungsloses User-Interface.
*   **Naming-Bereich:** User-ABI und globale API-Aufrufe.

### 1.2 Vera (Die Endministratorin / Main Kernel)
*   **Charakter:** Unerbittlich, streng, elegant (Sci-Fi-Vibe).
*   **Aufgabe:** Der Kern des Zero-Trust-Systems im Ring 0. Vera vertraut niemandem. Jede Ressourcen-Anfrage, jeder System Call und jede Rechteausweitung wird strikt geprüft. Sie überwacht das System, greift aber nicht aktiv in jeden User-Memory-Access ein (um Performance zu wahren), sondern erzwingt Regeln rigoros über Syscalls und MMU-Boundaries.
*   **Naming-Bereich:** Interne Kernel-Subsysteme, Memory Management, Security-Policies.

### 1.3 Lily (Die Weckerin / Pre-Stage)
*   **Charakter:** Organisch, fundamental.
*   **Aufgabe:** Weckt die Hardware auf, initialisiert Low-Level-Ressourcen (UART, Device Tree) und übergibt den kontrollierten State an Vera.
*   **Naming-Bereich:** Bootloader- und Pre-Kernel-Initialisierung.

---

## 2. Zero-Trust & Memory-Prinzipien

Vera setzt Sicherheit über alles, ohne dabei leichtfertig Performance zu opfern. Die Architektur vermeidet bekannte Angriffsvektoren durch experimentelle, aber hochwirksame Ansätze:

1.  **Out-of-Bound Memory Metadata:**
    *   Der Kernel-Allocator nutzt **keine** Inline-Metadaten neben den Speicherblöcken.
    *   Metadaten werden strikt isoliert in einer Hashmap verwaltet.
    *   **Vorteil:** Buffer-Overflows im User- oder Kernel-Space können keine Allocator-Header überschreiben.
2.  **Strict Syscall Enforcement:**
    *   Vera überwacht nicht jede einzelne Speicherleseoperation im Userland (Hardware-Paging übernimmt die Basis-Isolation).
    *   Sobald ein Prozess jedoch Systemressourcen fordert (Syscalls), Dateizugriffe anfragt oder mit anderen Prozessen interagieren will, wird die Anfrage auf Herz und Nieren verifiziert.
3.  **Dynamic Assembly / JIT Control:**
    *   Executable Memory (W^X) wird streng reguliert.
    *   Browser oder JIT-Compiler müssen für Dynamic Assembly explizit Berechtigungen anfragen, die standardmäßig beschränkt sind, sofern der Nutzer sie nicht aus Performance-Gründen freigibt.
4.  **Revocable Permissions & Offline Rules:**
    *   Kein Recht ist permanent. Jede Erlaubnis (z.B. Dateizugriff) kann Prozessen jederzeit entzogen werden.
    *   Offline-Regeln definieren Berechtigungen für Programme, selbst wenn diese gerade nicht ausgeführt werden.

---

## 3. Naming Conventions (Namensgebung)

Die Namen im Code sollen den Kontext sofort klarmachen. Die Zeiten allgemeiner `k_`-Prefixe sind vorbei.

### 3.1 Kernel (Vera)
Das Prefix lautet **`vera_`**. Es liest sich wie ein Regelwerk, bei dem "Vera" die Aktion ausführt.

**Struktur:** `vera_[subsystem]_[aktion]`

**Beispiele:**
*   `vera_mem_alloc()` / `vera_mem_free()` (Speicherverwaltung)
*   `vera_trust_revoke(pid)` (Zero-Trust Enforcement)
*   `vera_trap_enter()` / `vera_trap_return()` (Interrupts/Traps)
*   `vera_jit_protect()` (Security)

### 3.2 Pre-Stage (Lily)
Das Prefix lautet **`lily_`**.

**Beispiele:**
*   `lily_uart_init()`
*   `lily_boot_handoff()`

### 3.3 User ABI (Astrynn)
Globale System Calls, die von User-Programmen aufgerufen werden, erhalten das elegante **`AY_`** Prefix (oder `AS_`).

**Beispiele:**
*   `AY_WindowCreate()`
*   `AY_RequestService()`

---

## 4. Coding Style & C-Rules

Der Code muss debug-freundlich, logisch und skalierbar sein.

### 4.1 Variablen & Typen
*   **Format:** `lower_snake_case` für Variablen und Felder.
*   **Klarheit:** Bedeutungsvolle Namen. Extrem kurze Namen (`i`, `j`, `pc`, `sp`) sind nur für Counter oder CPU-Register erlaubt.
*   **Beispiele:** `size_t buffer_length`, `uint64_t kernel_ticks`.

### 4.2 Sichtbarkeit (`static` vs. `extern`)
*   **Grundregel:** Alles ist privat (`static`), bis es bewusst exportiert werden muss.
*   **`static` (in .c Files):** Zwingend für interne Hilfsfunktionen und private Modul-Zustände.
*   **`extern`:** Nur für gemeinsam genutzte, globale Variablen (sehr sparsam verwenden!). Definition in genau einer `.c` Datei, Deklaration im `.h` File.
*   **Public API:** Funktionen, die über Modulgrenzen hinweg genutzt werden, stehen im Header (`.h`) und haben kein `static`.

### 4.3 Inline-Policy
*   **Default:** Kein `inline`. Die Lesbarkeit von Stack-Traces beim Debuggen hat Vorrang.
*   **Ausnahme:** `static inline` ist ausschließlich für winzige, extrem häufig aufgerufene Einzeiler (z.B. Register-Checks, Status-Flags) gestattet.
*   Compiler-Optimierungen (`-O2` / `-O3`) übernehmen später das Inlining.

### 4.4 Datei-Organisation
Pro logischem Subsystem eine klare Trennung zwischen Schnittstelle und Implementierung.
*   `vera_mem.c` / `vera_mem.h` (Out-of-Bound Allocator, Hashmap)
*   `vera_trap.c` / `vera_trap.h` (Interrupt & Exception Handling)
*   `vera_trust.c` / `vera_trust.h` (Syscall-Verifikation, Security Policies)

---
> **Zukunftsausblick:** Wenn das System wächst, wird ein isolierter Fallback-Kernel als ultimatives Sicherheitsnetz implementiert. Die modulare `vera_`-Struktur bereitet den Code genau darauf vor.
