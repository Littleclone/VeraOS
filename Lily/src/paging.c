/* src/memory/paging.c */
#include "../header/paging.h"
#include "../header/mem_controller.h"
#include "../header/dtb_parser.h"
#include "../header/lily_extension.h"

// Helper Function Prototype

typedef enum
{
    paging_Valid = 1,     // V
    paging_Read = 2,      // R
    paging_Write = 4,     // W
    paging_Execute = 8,   // X
    paging_U_Mode = 16,   // U -> Darf U Mode zugreifen
    paging_Global = 32,   // G -> Ist für jede ASID zugänglich
    paging_Accessed = 64, // A -> Wenn nicht aktiv, gibt Page Fault wenn Accessed
    paging_Dirty = 128,   // D -> Wenn nicht gesetzt, gibt Page Fault wenn geschrieben wird.
} paging_flags_lily;

typedef struct
{
    page_table *root_table;
    uint8_t *next_free_table;
} paging_table_container_lily;

static inline uint64_t paging_make_pte_Sv39_lily(const uint64_t phys_address, const uint64_t flags);
static uint64_t paging_get_pages_needed_lily(const uint64_t size_of_region);
static void paging_map_pages_lily(paging_table_container_lily *const table_ptr, phys_address p_address, virt_address v_address, uint64_t needed_pages, const paging_flags_lily flags, const bool is_Global);

void paging_init_kernel_lily(kernel_boot_info *boot_info)
{

#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter paging_init_kernel_lily\n");
#endif
    if (boot_info == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }

    // Start now
    mem_node_lily *node_holder = (mem_node_lily *)boot_info->mem_nodes;
    mem_node_lily *target = NULL;
    dtb_tree_lily *tree = (dtb_tree_lily *)boot_info->dtb;
    for (uint16_t i = 0; i < boot_info->mem_nodes_counter; ++i)
    {
        if (target == NULL)
        {
            target = &node_holder[i];
            if (target->mem_state != mem_free_memory_dirty_lily)
            {
                target = NULL;
                continue;
            }
        }
        if (target->size < node_holder[i].size && node_holder[i].mem_state == mem_free_memory_dirty_lily)
        {
            target = &node_holder[i];
        }
    }

    if (target->mem_state != mem_free_memory_dirty_lily)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NOMEM);
#else
        lily_panic_no_write(VERA_ERR_NOMEM);
#endif
    }
    lily_utils_mem_set_64((uint64_t *)target->address, 0, target->size / 8);
    // We have a large Node now we put there our temporary Page Table
    paging_table_container_lily table;
    table.root_table = (page_table *)lily_utils_align((uint64_t)target->address, Page_Size);
    table.next_free_table = NULL;

    // Get the Kernel Info Holder Node
    for (uint16_t i = 0; i < boot_info->mem_nodes_counter; ++i)
    {
        target = &node_holder[i];
        if (target->mem_state == mem_reserved_kernel_info_lily)
        {
            break;
        }
        target = NULL;
    }

    if (target == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    // Map Kernel

    // Lily
    // Stack (Lily)
    // Stack Guard (Lily)

    // Kernel
    // Text
    paging_map_pages_lily(&table, (phys_address)boot_info->kernel_region.text_start, (virt_address)boot_info->kernel_region.text_start, paging_get_pages_needed_lily(boot_info->kernel_region.text_end - boot_info->kernel_region.text_start), Kernel_text_flags, true);

    // Data
    paging_map_pages_lily(&table, (phys_address)boot_info->kernel_region.data_start, (virt_address)boot_info->kernel_region.data_start, paging_get_pages_needed_lily(boot_info->kernel_region.data_end - boot_info->kernel_region.data_start), Kernel_data_flags, true);

    // RoData
    paging_map_pages_lily(&table, (phys_address)boot_info->kernel_region.rodata_start, (virt_address)boot_info->kernel_region.rodata_start, paging_get_pages_needed_lily(boot_info->kernel_region.rodata_end - boot_info->kernel_region.rodata_start), Kernel_roData_flags, true);

    // sData
    paging_map_pages_lily(&table, (phys_address)boot_info->kernel_region.sdata_start, (virt_address)boot_info->kernel_region.sdata_start, paging_get_pages_needed_lily(boot_info->kernel_region.sdata_end - boot_info->kernel_region.sdata_start), Kernel_data_flags, true);

    // bss
    paging_map_pages_lily(&table, (phys_address)boot_info->kernel_region.bss_start, (virt_address)boot_info->kernel_region.bss_start, paging_get_pages_needed_lily(boot_info->kernel_region.bss_end - boot_info->kernel_region.bss_start), Kernel_data_flags, true);

    // Stack (Kernel)
    paging_map_pages_lily(&table, (phys_address)boot_info->kernel_region.stack_start, (virt_address)boot_info->kernel_region.stack_start, paging_get_pages_needed_lily(boot_info->kernel_region.stack_end - boot_info->kernel_region.stack_start), Kernel_data_flags, true);

    // Stack Guard (Kernel) (roData Flag, only Read Allowed)
    paging_map_pages_lily(&table, (phys_address)boot_info->kernel_region.stack_guard_start, (virt_address)boot_info->kernel_region.stack_guard_start, paging_get_pages_needed_lily(boot_info->kernel_region.stack_guard_end - boot_info->kernel_region.stack_guard_start), Kernel_roData_flags, true);

    // Map Temp Heap and Boot Info
    // temp Heap
    paging_map_pages_lily(&table, (phys_address)boot_info->temp_heap, (virt_address)boot_info->temp_heap, paging_get_pages_needed_lily(boot_info->temp_heap_size), Kernel_data_flags, true);

    // Boot Info
    paging_map_pages_lily(&table, (phys_address)boot_info, (virt_address)boot_info, paging_get_pages_needed_lily(target->size), Kernel_data_flags, true);

    // Map Aditional things like DTB and if QEMU & DEBUG the UART
    paging_map_pages_lily(&table, (phys_address)tree, (virt_address)tree, paging_get_pages_needed_lily((lily_utils_swap_endian32(tree->header.totalsize))), Kernel_roData_flags, true);
#if INCLUDE_DEBUG && INCLUDE_QEMU
    paging_map_pages_lily(&table, (phys_address)UART_BASE_QEMU, UART_BASE_QEMU, 1, Kernel_data_flags, true);
#endif

#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Activating Paging\n");
#endif
    activatePaging_Lily((uint64_t*)table.root_table, Kernel_Process_ID);
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Activ Paging\n");
#endif

#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave paging_init_kernel_lily\n");
#endif
}

/*
Initialisiert die Root Page Table und das Komplette Paging, hier werden neue PTE hinzugefügt
Braucht ein Paging_Table_container und der bereich wo das Paging sein soll muss Nulled (0) werden

params:
- (const) (ptr) table_ptr -> Der Pointer zur Page Table des Process
- p_address -> Die Physische adresse bei der wir beginnen
- v_address -> Die Virtuelle Adresse bei der wir beginnen
- needed_pages -> Die Anzahl an Pages, die dieses Paging Setup abdecken wird.
- (const) flags -> Die flags die den Leaf Pages gegeben werden
- (const) is_global -> Setzt flags bei PTE zu Global dazu
- (const) to_virt -> phys Pointer werden zu virt pointer

return:
- VERA_OK: Alles okay
- K_ERR_NULL_PTR: Null Pointer wurden übergeben
- VERA_ERR_INVAL: Wenn p_address oder v_address 0 sind
- VERA_ERR_PERM: Wenn die v_addresse in ungültige bereiche ist.
- VERA_ERR_NOSUP: Falls die Leaf PTE nicht 0 ist obwohl es dies sollte
*/
static void paging_map_pages_lily(paging_table_container_lily *const table_ptr, phys_address p_address, virt_address v_address, uint64_t needed_pages, const paging_flags_lily flags, const bool is_Global)
{
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter paging_map_pages_lily\n");
#endif
    page_table *root_table_ptr = table_ptr->root_table;
    if (root_table_ptr == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    if (p_address == 0 || v_address == 0)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    // Im Bestfall nicht nötigt, Refactor weg sobald woanders dies überprüft wird! TODO
    if ((uint64_t)v_address > VA_LOWER_HALF_END && (uint64_t)v_address < VA_UPPER_HALF_START)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_PERM);
#else
        lily_panic_no_write(VERA_ERR_PERM);
#endif
    }
    uint8_t *next_table = table_ptr->next_free_table;
    if (next_table == NULL)
    {
        next_table = (uint8_t *)&root_table_ptr[Max_entry];
    }
    while (needed_pages)
    {
        // Anfang
        uint16_t vpn_2 = (uint16_t)((uint64_t)v_address >> 30) & 0x1ff;
        uint16_t vpn_1 = (uint16_t)((uint64_t)v_address >> 21) & 0x1ff;
        uint16_t vpn_0 = (uint16_t)((uint64_t)v_address >> 12) & 0x1ff;
        // PT_Root
        page_table_entry *root_page_entry = &root_table_ptr[vpn_2];
        page_table *level_1_table = NULL;
        // If kein Eintrag
        if (*root_page_entry == 0)
        {
            level_1_table = (page_table *)next_table;
            next_table += Page_Size;
            for (uint16_t i = 0; i < Max_entry; ++i)
            {
                level_1_table[i] = 0;
            }
            *root_page_entry = paging_make_pte_Sv39_lily((uint64_t)level_1_table, (is_Global) ? (paging_Valid | paging_Global) : paging_Valid);
        }
        else
        {
            level_1_table = (page_table *)((*root_page_entry >> 10) << 12);
        }
        // PT_Level_1
        page_table_entry *level_1_page_entry = &level_1_table[vpn_1];
        page_table *level_0_table = NULL;
        // If kein Eintrag
        if (*level_1_page_entry == 0)
        {
            level_0_table = (page_table *)next_table;
            next_table += Page_Size;
            for (uint16_t i = 0; i < Max_entry; ++i)
            {
                level_0_table[i] = 0;
            }
            *level_1_page_entry = paging_make_pte_Sv39_lily((uint64_t)level_0_table, (is_Global) ? (paging_Valid | paging_Global) : paging_Valid);
        }
        else
        {
            level_0_table = (page_table *)((*level_1_page_entry >> 10) << 12);
        }
        // PT_Level_0
        uint64_t pages = needed_pages;
        for (uint16_t i = vpn_0; 0 < pages; ++i)
        {
            if (i > 511 || needed_pages <= 0)
            {
                break;
            }
            page_table_entry *leaf_page_entry = &level_0_table[i];
            if (*leaf_page_entry != 0)
            {
#if INCLUDE_DEBUG && INCLUDE_QEMU
                lily_panic(VERA_ERR_NOSUP);
#else
                lily_panic_no_write(VERA_ERR_NOSUP);
#endif
            }
            // Add Leaf Pages
            *leaf_page_entry = paging_make_pte_Sv39_lily((uint64_t)p_address, flags);
            p_address += 0x1000;
            v_address += 0x1000; // Wir gehen eine Page weiter
            --needed_pages;
        }
        // Ende
    }
    table_ptr->next_free_table = next_table;
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave paging_map_pages_lily\n");
#endif
}

// Helper Funktionen um Paging zu editieren

// Helper Function

/*
Erstellt den PTE gerecht für den Standart Sv39

params:
- (const) phys_address -> Die addresse auf die der pte zeigt
- (const) flags -> Die flags die der pte hat

return:
- der fertige pte
*/
static inline uint64_t paging_make_pte_Sv39_lily(const uint64_t phys_address, const uint64_t flags)
{
    uint64_t ppn = phys_address >> 12;
    return (ppn << 10) | flags;
}

/*
Berechnet aus anhand der Bytes wie viele Pages gebraucht werden

params:
- size_of_region -> Die Size der Region in Bytes

return:
- Anzahl der Pages die Process X braucht.
*/
static uint64_t paging_get_pages_needed_lily(const uint64_t size_of_region)
{
    uint64_t padding_pages = size_of_region % Page_Size;
    uint64_t pages = size_of_region / Page_Size;
    if (padding_pages)
    {
        ++pages;
    }
    return pages;
}
