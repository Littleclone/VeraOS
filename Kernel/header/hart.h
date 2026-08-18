#pragma once

#include "Vera_Utils/utils.h"

typedef struct
{
    // --- 1. Basis-Informationen ---
    uint64_t hart_id; // Aus 'reg' (Deine eindeutige CPU-Nummer)
    char *status;     // Aus 'status' (meist "okay" oder "disabled")

    // --- 2. Takt & Frequenzen ---
    uint64_t timebase_frequency; // Aus /cpus (Wichtig für den Scheduler & sleep)
    uint64_t clock_frequency;    // Aus CPU Node (CPU-Takt, nützlich für UI-Anzeigen, oft 0 falls fehlt)

    // --- 3. Speicher & Paging ---
    char *mmu_type; // Aus 'mmu-type' (z.B. "riscv,sv39" oder "riscv,sv57")

    // --- 4. Lokaler Interrupt-Controller (Wichtigste ID für Treiber!) ---
    uint32_t intc_phandle; // Das phandle des 'interrupt-controller' Sub-Knotens

    // --- 5. Cache Management (Überlebenswichtig für DMA & Treiber) ---
    uint32_t cbom_block_size; // riscv,cbom-block-size (Für Cache-Flushes)
    uint32_t cbop_block_size; // riscv,cbop-block-size (Für Prefetching)
    uint32_t cboz_block_size; // riscv,cboz-block-size (Um RAM schnell mit Nullen zu füllen)

    // --- 6. CPU-Features (ISA) ---
    char *isa_string; // Raw-String aus 'riscv,isa' (z.B. "rv64imafd...")

    // Praktisches Bitfeld: Es ist effizienter, den isa_string EINMAL beim Parsen
    // auszuwerten und als Booleans zu speichern. Dein Kernel kann dann später
    // extrem schnell prüfen: if (hart->features.has_f) { ... }
    struct
    {
        // ---------------------------------------------------------
        // DEINE MUST-HAVES (Die Basis für dein Alltags-OS)
        // ---------------------------------------------------------
        uint64_t has_m : 1;        // [Must] Integer Multiplikation/Division ("m")
        uint64_t has_a : 1;        // [Must] Atomics für Spinlocks/Mutexes ("a")
        uint64_t has_c : 1;        // [Must] Compressed Instructions, spart Platz ("c")
        uint64_t has_bitmanip : 1; // [Must] Bit Shifts & Manipulation ("zba", "zbb", "zbc", "zbs")
        uint64_t has_ssaia : 1;    // [Must] APLIC Support im S-Mode ("ssaia")

        // ---------------------------------------------------------
        // KERNEL "MUST-HAVES" (Ohne die wird S-Mode schmerzhaft)
        // ---------------------------------------------------------
        uint64_t has_sstc : 1; // [Wichtig] Erlaubt dir den Timer direkt im S-Mode zu setzen ("sstc")

        // ---------------------------------------------------------
        // NICE-TO-HAVE (Features für User-Programme & Performance)
        // ---------------------------------------------------------
        uint64_t has_f : 1;      // [Nice] Single Float. Wichtig zu wissen für Context-Switches ("f")
        uint64_t has_d : 1;      // [Nice] Double Float ("d")
        uint64_t has_v : 1;      // [Nice] Vector Extensions ("v")
        uint64_t has_svadu : 1;  // [Nice] Hardware übernimmt das Paging A/D-Bit ("svadu")
        uint64_t has_zicbom : 1; // [Nice] Cache-Management für DMA/Netzwerkkarten ("zicbom")
        uint64_t has_zicboz : 1; // [Nice] Blockweises Nullen von neuem RAM ("zicboz")
        uint64_t has_zawrs : 1;  // [Nice] CPU schläft bei Spinlocks, statt Strom zu fressen ("zawrs")

        // Restliche 51 Bits sind automatisch für zukünftige Extensions reserviert
    } hart_features_t;

} hart_info_struct_deep;

vera_state k_init_harts(vera_utils_safe_array* hart_array);

void hart_init_function_flags(hart_info_struct_deep* hart, uint32_t length);

extern void k_init_test_timer(void);