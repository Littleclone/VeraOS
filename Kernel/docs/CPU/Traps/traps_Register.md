# stvec = (Supervisor Trap Vector)
Dieser Register hält die Adresse zum Trap Handler wohin die CPU Springt bei einer Trap

Lade Adresse vom Label und schreib es in stvec, beispiel:

```
lla t0, trap_entry 
csrw stvec, t0
```
Hinweis: Bit 1:0 bestimmen den Modus.
0 = Direct (alle Traps gehen zu trap_entry)
1 = Vectored (Interrupts = base + 4×cause)

Danach nicht mehr anfassen

---

# sstatus 

Brauche nur 3 Bits (Anfang)

### Bit 1 (SIE) - Supervisor Interrupt Enable

Wenn "1" dann sind Interrupts Global Aktiv (in S Mode), wenn "0" sind sie Deaktiviert \
Sollte eine Trap ausgelöst werden wird diese im Core wo sie ausgelöst wurde auf "0" gesetzt, bei sret wird es automatisch wieder auf "1" gesetzt. (Mehr dazu schaue zu Bit 5)

```
csrr t0, sstatus
ori t0,  t0, 1 << 1
csrw sstatus, t0
```

### Bit 5 (SPIE) - Saved Interrupt Enable

Wird von der CPU automatisch gespeichert und Managed.

Bei Trap Entry kriegt SPIE den wert von SIE, SIE wird dann auf "0" gesetzt, bei sret wird in SIE den wert von SPIE gesetzt, in unserem Fall "1".

```
Trap Entry:
SPIE = SIE
SIE = 0
```
```
sret:
SIE = SPIE
```

Nicht Manuel bearbeiten!

### Bit 8 (SPP) - Saved Previous Privilege

Dieser Bit sagt einfach von wo die Trap ausgelöst wurde, wenn die Trap im U-Mode ausgelöst wurde ist der Bit "0", sollte die Trap im S-Mode ausgelöst werden ist der Bit "1". \
Dieser Bit entscheidet auch in welchen Mode es wechselt nach sret.
```
sret:
SPP = 1 -> Geht in S-Mode (oder bleibt falls schon darin)
SPP = 0 -> Geht in U-Mode
```

---
# sepc

Die CPU Speichert hier die Adresse (PC) wo sie unterbruchten wurde, dies ist notwendig damit wenn sret benutzt wird die CPU wieder zurück zur unterbrochenen Stelle kommt.

Die CPU Setzt sepc automatisch bei einer Trap. \
Da keine Interrupts nestled werden sollte sich sepc normal nicht ändern während des bearbeiten eines Interrupts, es lohnt sich diesen aber vorsichtshalber dennoch zu sichern falls der Kernel selbst Trap's auslöst und sepc überschreibt, da dieser nur ein wert halten kann.

### Syscall
Bei Syscall lohnt es sich den PC um 4 Bytes (RISC-V Standart OpCode Größe ist 32-Bit) zu addieren um zur nächsten Instruction zu kommen (und nicht endloss ausgeführt wird), könnte sich auch bei anderen dingen Lohnen.

---
# scause

Hier speichert die CPU automatisch informationen über den Grund der Trap, beispiel obs ein Syscall war, ein Timer Interrupt oder weiteres. (Die Liste findest du bei docs/CPU/Traps/trap_reason.md)

### Bit 63(MSB)

Dieser Bit speichert ob es sich um ein Interrupt oder eine Exception handelt, bei einem Interrupt ist der Bit "1", bei einer Exception "0".

### Bits 0-62

Hier ist die Nummer die besagt was der grund der Trap war. (Bit 63 muss raus AND werden)

---
# stval
Die CPU gibt und hier automatisch zusätzliche Informationen zu bestimmten Traps, default wert ist "0". (Der Default wert ist drin wenn keine Zusatz Infos vorliegen)

### Beispiel
| Ausnahme               | stval-Inhalt                        |
| ---------------------- | ----------------------------------- |
| Instruction page fault | fehlerhafte Instruktionsadresse     |
| Load page fault        | fehlerhafte virtuelle Load-Adresse  |
| Store/AMO page fault   | fehlerhafte virtuelle Store-Adresse |
| Misaligned access      | fehlerhafte Adresse                 |
| Access fault           | fehlerhafte Adresse                 |
| Illegal instruction    | 32-Bit Instruktionswort             |
| Breakpoint             | Adresse der Breakpoint-Instruction  |
| ecall / Interrupts     | **0** (nie gesetzt)                 |


---
# sie - Enable Spezifische Interrupts
Hier kann man bestimmen welche Interrupts überhaupt eine Trap auslösen sollen.

### Bit 1 (SSIE) - Supervisor Software Interrupt Enable
Software Interrupts. \
Kann ausgelöst werden durch:
```
Durch den Kernel Selbst: 
OpenSBI = sbi_send_ipi();
```
```
Durch einen anderen CPU-Core (Inter-Process Interrupt, IPI):
Core 0 schickt Software-Interrupt an Core 1
Core 1 bekommt SSIP = 1 -> löst Trap aus
```
Eine art Signal oder Wake-up Mechanismus. (oder wie auch immer du es nutzen willst)

### Bit 5 (STIE) - Supervisor Timer Interrupt Enable
Timer Interrupt. \
Wird durch OpenSBI ausgelöst (bei QEMU) in dem es STIP = 1. \
Auf Echten Boards kann es eigene SoC Timer-Hardware geben der STIP = 1 macht.

### BIT 9 (SEIE) - Supervisor External Interrupt Enable
Hardware Interrupt. \
Diese wird durch PLIC (Platform Level Interrupt Controller) ausgelöst. Sind Hardware Interrupts. \
Setzt SEIP = 1 bei Hardware Interrupt

---

# sip - Pending Bits
CPU oder PLIC setzen diese bits um zu sagen:
"Hey, da ist ein Interrupt!"

Muss man nie lesen, ist Optional.

### Bit 1 (SSIP) - Software Interrupt pending
Wird von OS/CPU gesetzt.

### Bit 5 (STIP) - Timer Interrupt pending
Wird von Timer/CPU gesetzt.

### Bit 9 (SEIP) - External Interrupt pending
Wird von PLIC gesetzt.

---

# sscratch
Hier kann man eine sache speichern die man will, gut nutzbar für Stack oder CPU Struct.

Beispiel Stackwechsel:
```
csrrw sp, sscratch, sp      # Tauscht user sp <-> kernel sp
```

Ich werde aber wahrscheinlich so machen das die Temporären Register im User Stack gespeichert werden damit ich diese danach nutzen kann um mein Kernel Stack aus dem CPU Struct aus sscratch zu holen, da wird dann alles andere gespeichert für den derzeit Laufenden Prozess (Dies ist wenn U-Mode machte ein Syscall)

---
