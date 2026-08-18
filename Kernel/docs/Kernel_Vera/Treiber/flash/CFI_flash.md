# CFI01 Flash Memory (Intel Standard) - Bare Metal Spezifikation

## ⚠️ Regel 1: Die Bus-Breite (Address-Multiplier)
Flash-Chips können unterschiedlich breit angebunden sein (8-Bit, 16-Bit oder 32-Bit). QEMU (`virt`) nutzt in der Regel einen **32-Bit breiten Bus**. 
Das ändert für dich zwei Dinge elementar:

1. **Die Datentypen:** Du liest und schreibst Befehle **immer** in der vollen Bus-Breite. Bei 32-Bit nutzt du in C also immer `uint32_t` Pointer.
2. **Der Adress-Multiplikator:** Die Offsets in offiziellen Datenblättern (wie `0x55` für den Query-Modus) beziehen sich immer auf die *Bus-Breite*, nicht auf nackte Bytes! Du musst den Offset mit deiner Bus-Breite in Bytes multiplizieren.

| Bus-Breite (Hardware) | Dein C-Datentyp | Offset-Multiplikator | Echte Adresse für Befehl `0x55` |
| :--- | :--- | :--- | :--- |
| **8-Bit** (x8) | `uint8_t` | `* 1` | `BASE + 0x55` |
| **16-Bit** (x16) | `uint16_t` | `* 2` | `BASE + 0xAA` |
| **32-Bit** (x32) [QEMU] | `uint32_t` | `* 4` | `BASE + 0x154` |

*(Beispiel QEMU 32-Bit: Um `0x98` an Offset `0x55` zu schicken, schreibst du den 32-Bit Wert `0x00000098` an die Adresse `BASE + 0x154`).*

## ⚠️ Regel 2: Wie groß ist ein Schreibvorgang?
Schreibt man immer nur 1 Byte? **Nein.** Du schreibst immer exakt **ein Flash-Word** pro Befehl. Ein Flash-Word entspricht deiner Bus-Breite. 
* Bei einem 32-Bit Bus schreibst du mit dem `0x40`-Befehl immer exakt **4 Bytes auf einmal**.
* Willst du eine Datei mit 1.000 Bytes in den Flash (32-Bit) schreiben, musst du den "Aktion 3: Daten Schreiben"-Workflow exakt 250 mal in einer Schleife ausführen. Nach *jedem* Word (4 Bytes) musst du zwingend warten, bis der Chip fertig ist!

## 🧠 Regel 3: Lesen, Schreiben, Löschen
* **Lesen:** Funktioniert exakt wie bei normalem RAM. Pointer drauf, Wert auslesen.
* **Löschen (Erase):** Ein gelöschter Bereich besteht immer aus `0xFF` (alle Bits sind `1`).
* **Schreiben (Program):** Du kannst Bits nur von `1` auf `0` kippen. Willst du einen Wert überschreiben und wieder eine `1` an einer Stelle haben, **musst du den kompletten Sektor vorher löschen**.

---

## 📜 Die wichtigsten Befehle (Hex-Werte)

Alle Befehle werden an den Chip gesendet, indem du diese Werte an die Basisadresse (oder Zieladresse) schreibst.

| Befehl | Hex-Wert | Was er tut |
| :--- | :--- | :--- |
| **Reset / Read Mode** | `0xFF` | Beendet alle Befehle. Chip verhält sich wieder wie normaler RAM zum Lesen. |
| **CFI Query** | `0x98` | Versetzt den Chip in den Info-Modus (Gibt ID und Größe preis). |
| **Block Erase** | `0x20` -> `0xD0` | Löscht einen kompletten Block (Macht alle Bytes zu `0xFF`). |
| **Word Program** | `0x40` | Sagt dem Chip: "Der nächste Wert, den ich schreibe, soll dauerhaft gespeichert werden." |
| **Read Status** | `0x70` | Fragt ab: "Bist du fertig mit Löschen/Schreiben?" |

---

## 🛠️ Die 4 Workflows

Geh davon aus, dass `BASE` deine Startadresse ist (z.B. `0x20000000`) und du auf einem **32-Bit Bus** operierst (Multiplikator * 4).

### Aktion 1: Das Lebenszeichen prüfen ("CFI Query")
Damit testest du, ob der Chip ansprechbar ist und den CFI-Standard versteht.

1. **Info-Modus starten:** Schreibe `0x98` an `BASE + (0x55 * 4)`. 
2. **Q prüfen:** Lies den Wert an `BASE + (0x10 * 4)`. Es muss ein `'Q'` (Hex `0x51`) sein.
3. **R prüfen:** Lies den Wert an `BASE + (0x11 * 4)`. Es muss ein `'R'` (Hex `0x52`) sein.
4. **Y prüfen:** Lies den Wert an `BASE + (0x12 * 4)`. Es muss ein `'Y'` (Hex `0x59`) sein.
5. **Modus beenden:** Schreibe `0xFF` an `BASE`, um zurück in den normalen Lese-Modus zu wechseln.

### Aktion 2: Einen Block löschen (Zwingend vor dem Schreiben!)
Du willst an die Adresse `ZIEL_ADRESSE` etwas schreiben? Lösche den Block, in dem die Adresse liegt, zuerst. *(Achtung: Flash löscht immer in großen Blöcken, z.B. 64KB, 128KB oder 256KB gleichzeitig!)*

1. **Vorbereitung:** Schreibe `0x20` an die `ZIEL_ADRESSE`.
2. **Bestätigung:** Schreibe `0xD0` an die `ZIEL_ADRESSE`. *(Löschvorgang startet jetzt in der Hardware!)*
3. **Warten:** Führe *Aktion 4 (Status Polling)* aus, bis der Chip fertig ist.
4. **Modus beenden:** Schreibe `0xFF` an `BASE`.

### Aktion 3: Daten schreiben (Word Program)
Der Bereich ist jetzt gelöscht (`0xFF`). Jetzt schreiben wir deinen Wert `DEIN_32BIT_WERT` an die `ZIEL_ADRESSE`.

1. **Kommando senden:** Schreibe `0x40` an die `ZIEL_ADRESSE`. *(Sagt dem Chip: Achtung, echte Daten kommen gleich).*
2. **Daten senden:** Schreibe `DEIN_32BIT_WERT` an die `ZIEL_ADRESSE`.
3. **Warten:** Führe *Aktion 4 (Status Polling)* aus, bis der Chip diese 4 Bytes gespeichert hat.
4. *(Schritt 1 bis 3 für die nächsten 4 Bytes wiederholen, falls du mehr schreiben willst).*
5. **Modus beenden:** Schreibe `0xFF` an `BASE`, um wieder normal lesen zu können.

### Aktion 4: Das Warten (Status Register Polling)
Hardware ist millionenmal langsamer als deine CPU. Nach jedem Löschen und nach jedem einzelnen Schreibvorgang (Word) MUSST du diese Schleife laufen lassen.

1. **Status anfordern:** Schreibe `0x70` an `BASE`. 
2. **Status lesen:** Lies den Wert von `BASE` in einer Schleife aus.
3. **Prüfen:** Maskiere den gelesenen Wert mit dem 7. Bit (`Wert & 0x80`).
   * Ist das Ergebnis `0`: Der Chip arbeitet noch. Schleife weiterlaufen lassen.
   * Ist das Ergebnis `1` (bzw. `0x80`): Der Chip ist fertig! Du darfst die Schleife beenden und den nächsten Befehl senden.