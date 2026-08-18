/* src/Vera_Debug/debug.c */
#include "../../header/Vera_Debug/debug.h"
#include "../../header/Vera_UART/uart.h"

static void print_pte(uint64_t page_table_entry);

/*
Emuliert in Software wie die MMU berechnet um an den gewünschten PTE zu kommen

Panic:
- VERA_ERR_INVAL: Root Page Table Entry ist leer
- VERA_ERR_INVAL: Level 1 Page Table Entry ist leer
- VERA_ERR_INVAL: Level 0 Page Table Entry (Leaf) ist leer
*/
void debug_mmu_simulation(uint64_t* root_table, uint64_t v_address_internal) {
    root_table = (uint64_t*)((uint64_t)root_table << 12);
    uint16_t vpn_2 = (uint16_t)((uint64_t)v_address_internal >> 30) & 0x1ff;
    uint16_t vpn_1 = (uint16_t)((uint64_t)v_address_internal >> 21) & 0x1ff;
    uint16_t vpn_0 = (uint16_t)((uint64_t)v_address_internal >> 12) & 0x1ff;
    uint16_t offset = (uint16_t)v_address_internal & 0xfff;
    vera_uart_printf("Anfang\nvpn2: %i, vpn1: %i, vpn0: %i\n", vpn_2, vpn_1, vpn_0);
    uint64_t* page_table_entry = &root_table[vpn_2];
    if ((*page_table_entry & 1) == 0) {
        vera_err_boot_panic(VERA_ERR_INVAL);
    }
    print_pte(*page_table_entry);
    // Table 1
    uint64_t* page_table = (uint64_t*)((*page_table_entry >> 10) << 12);
    page_table_entry = &page_table[vpn_1];
    if ((*page_table_entry & 1) == 0) {
        vera_err_boot_panic(VERA_ERR_INVAL);
    }
    print_pte(*page_table_entry);
    // Table 0
    page_table = (uint64_t*)((*page_table_entry >> 10) << 12);
    page_table_entry = &page_table[vpn_0];
    if ((*page_table_entry & 1) == 0) {
        vera_err_boot_panic(VERA_ERR_INVAL);
    }
    print_pte(*page_table_entry);
    uint64_t address_needed = (*page_table_entry >> 10) << 12;
    address_needed |= offset;
    vera_uart_printf("Offset Added: %p\n", address_needed);
}

/*
Gibt nützliche Informationen über den derzeitigen PTE aus, wird für die MMU Simulation genutzt
*/
static void print_pte(uint64_t page_table_entry) {
    bool is_valid = (page_table_entry & 1);
    bool is_read = (page_table_entry & 2);
    bool is_write = (page_table_entry & 4);
    bool is_execute = (page_table_entry & 8);
    bool is_U_mode = (page_table_entry & 16);
    bool is_global = (page_table_entry & 32);
    bool is_accessed = (page_table_entry & 64);
    bool is_dirty = (page_table_entry & 128);
    uint64_t ppn = (page_table_entry >> 10);
    uint64_t phy_address = (ppn << 12);

    vera_uart_printf("\nNew PTE:\nis_Valid = %i\nis_Read = %i\nis_Write: %i\nis_Execute: %i\n", is_valid, is_read, is_write, is_execute);
    vera_uart_printf("is_U-Mode: %i\nis_global: %i\nis_Accessed: %i\nis_Dirty: %i\n", is_U_mode, is_global, is_accessed, is_dirty);
    vera_uart_printf("ppn: %ilx\nphy_address: %ilx\n", ppn, phy_address);
}