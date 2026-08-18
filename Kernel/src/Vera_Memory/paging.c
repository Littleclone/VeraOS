/* src/memory/paging.c */
#include "../../header/Vera_Memory/paging.h"
#include "../../header/Vera_Memory/mem_controller.h"
#include "../../header/Vera_UART/uart.h"

// Helper Function Prototype

static inline uint64_t paging_make_pte_Sv39(uint64_t phys_address, uint64_t flags);

static uint64_t vera_paging_get_level1_pages_amount(uint64_t size_of_region);
static uint64_t vera_paging_get_level2_pages_amount(uint64_t size_of_region);

/*
    Initialises an Page Root Table (Or adds new entries) for a Process Page Table, the Initialisiation needs
    the Page Table to be nulled (Zeroed out)

    function description:
    - The function walks through the entire Page Table to set every PTE at its place, there for we get new Memory areas
    and placing new PTE's that points to new PT a Level under for the new PTE's until we have the Leaf Pages, if a Page Table
    already exists it will be loaded and then a new PTE Added. Its really is a walk through Maybe not the best effizient code, 
    but a realable code.

    params:
    - (const) (ptr) root_table -> The Pointer to the Process Page Table Container
    - p_address -> The Physical Address we beginn
    - v_address -> The Virtuell Address we beginn
    - needed_pages -> The Amount of Pages we need to map the entire Process
    - flags -> The Flags for the Leaf Pages in the End
    - is_global -> Decides if the PTE gets the Global Flag set
    - to_virt -> If true makes sure that the Pointers are transformed from Physical to Virtuell, so our Code can work with it.

    params check:
    - if root_table_ptr == NULL -> return VERA_ERR_NULL_PTR;
    - if p_address or v_address == 0 -> return VERA_ERR_INVAL;
    - if v_address > `VA_LOWER_HALF_END` and v_address < `VA_UPPER_HALF_START` is true -> return VERA_ERR_PERM;

    return:
    - VERA_OK: Everything fine
    - VERA_ERR_NULL_PTR: NULL Pointer as Parameter
    - VERA_ERR_INVAL: P_Address or/and V_Address are 0
    - VERA_ERR_PERM: If V_Address is in an invalid Memory area for Sv39
    - VERA_ERR_NOSUP: If Leaf Page is not Zero but it should be set by this function
*/
vera_state paging_map_pages_Sv39(paging_table_container *const table_ptr, phys_address p_address, virt_address v_address, uint64_t needed_pages, const paging_flags flags, const bool is_Global, const bool to_virt)
{
    #if INCLUDE_DEBUG && INCLUDE_QEMU
    vera_uart_print("Enter paging_map_pages\n");
    #endif
    page_table *root_table_ptr = table_ptr->root_table;
    if (root_table_ptr == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
    if (p_address == 0 || v_address == 0)
    {
        return VERA_ERR_INVAL;
    }
    // Im Bestfall nicht nötigt, Refactor weg sobald woanders dies überprüft wird! TODO
    if ((uint64_t)v_address > VA_LOWER_HALF_END && (uint64_t)v_address < VA_UPPER_HALF_START)
    {
        return VERA_ERR_PERM;
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
        if (to_virt)
        {
            root_page_entry = (uint64_t *)PAGING_Phys_to_Virt((uint64_t)root_page_entry)
        }
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
            if (to_virt)
            {
                level_1_table = (uint64_t *)PAGING_Virt_to_Phys((uint64_t)level_1_table)
            }
            *root_page_entry = paging_make_pte_Sv39((uint64_t)level_1_table, (is_Global) ? (paging_Valid | paging_Global) : paging_Valid);
        }
        else
        {
            level_1_table = (page_table *)((*root_page_entry >> 10) << 12);
        }
        // PT_Level_1
        page_table_entry *level_1_page_entry = &level_1_table[vpn_1];
        if (to_virt)
        {
            level_1_page_entry = (uint64_t *)PAGING_Phys_to_Virt((uint64_t)level_1_page_entry)
        }
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
            if (to_virt) {
                level_0_table = (uint64_t*)PAGING_Virt_to_Phys((uint64_t)level_0_table);
            }
            *level_1_page_entry = paging_make_pte_Sv39((uint64_t)level_0_table, (is_Global) ? (paging_Valid | paging_Global) : paging_Valid);
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
            if (to_virt)
            {
                leaf_page_entry = (uint64_t *)PAGING_Phys_to_Virt((uint64_t)leaf_page_entry)
            }
            if (*leaf_page_entry != 0)
            {
                vera_uart_printf("TT: %p, %p\n", *leaf_page_entry, leaf_page_entry);
                vera_err_boot_panic(VERA_ERR_NOSUP);
            }
            // Add Leaf Pages
            *leaf_page_entry = paging_make_pte_Sv39((uint64_t)p_address, flags);
            p_address += 0x1000;
            v_address += 0x1000; // Wir gehen eine Page weiter
            --needed_pages;
        }
        // Ende
    }
    table_ptr->next_free_table = next_table;
    #if INCLUDE_DEBUG && INCLUDE_QEMU
    vera_uart_print("Leave paging_map_pages\n");
    #endif
    return VERA_OK;
}

// Helper Funktionen um Paging zu editieren

/* Editiert Page Table Entries, beendet (mit K_OK)
braucht ein bereits Initialisierte Root Page Table, kann nur Flags und Physische Addressen ändern

params:
- (const) (ptr) root_table -> Der Pointer zur Page Table des Process
- (const) v_address -> Die Virtuelle Addresse die wir ändern wollen
- (const) new_p_address -> Wir können damit einer PTE oder Leaf zu einer neuen physical_addresse bringne
- flags -> Die flags die dem Leaf gegeben werden sollen (wenn 0 dann bleibden die Flags erhalten) und obs PTE2 oder PTE1 ändern soll

Return:
- K_OK: Alles in Ordnung
- VERA_ERR_NULL_PTR: Null pointer
- VERA_ERR_INVAL -> PTE/Leaf ist nicht genutzt

Note:
- ASID muss sich ändern damit die MMU keine falschen werte sieht (Bei Global oder ASID End, Flush muss genutzt werden)
*/
vera_state paging_edit_page_Sv39(paging_table_container *const var_root_table, virt_address v_address, phys_address new_p_address, paging_flags flags, bool to_virt)
{
    if (var_root_table == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
#if INCLUDE_DEBUG
    vera_uart_print("Enter paging_edit_page_Sv39\n");
#endif
    // VPN's erstellen
    uint16_t vpn_2 = (uint16_t)((uint64_t)v_address >> 30) & 0x1ff;
    uint16_t vpn_1 = (uint16_t)((uint64_t)v_address >> 21) & 0x1ff;
    uint16_t vpn_0 = (uint16_t)((uint64_t)v_address >> 12) & 0x1ff;

    // Tables und PTE kriegen
    page_table *root_table = var_root_table->root_table;
    page_table_entry *root_pte = &root_table[vpn_2];

    if ((flags & paging_is_pte2))
    {
        flags ^= paging_is_pte2;
        uint64_t pte_address = 0;
        if (to_virt)
        {
            root_pte = (page_table_entry *)PAGING_Phys_to_Virt((uint64_t)root_pte)
        }
        if (*root_pte == 0)
        {
            return VERA_ERR_INVAL;
        }
        if (new_p_address != 0)
        {
            pte_address = new_p_address;
        }
        else
        {
            pte_address = (*root_pte >> 10) << 12;
        }
        if (flags == 0)
        {
            flags = (*root_pte << 53) >> 53;
        }
        *root_pte = paging_make_pte_Sv39(pte_address, flags);
    }
    else if ((flags & paging_is_pte1))
    {
        flags ^= paging_is_pte1;
        if (to_virt)
        {
            root_pte = (page_table_entry *)PAGING_Phys_to_Virt((uint64_t)root_pte)
        }
        if (*root_pte == 0)
        {
            return VERA_ERR_INVAL;
        }
        page_table *level_1_table = (page_table *)((*root_pte >> 10) << 12);
        page_table_entry *level_1_pte = &level_1_table[vpn_1];
        if (to_virt)
        {
            level_1_pte = (page_table_entry *)PAGING_Phys_to_Virt((uint64_t)level_1_pte)
        }
        if (*level_1_pte == 0)
        {
            return VERA_ERR_INVAL;
        }
        uint64_t pte_address = 0;
        if (new_p_address != 0)
        {
            pte_address = new_p_address;
        }
        else
        {
            pte_address = (*level_1_pte >> 10) << 12;
        }
        if (flags == 0)
        {
            flags = (*level_1_pte << 53) >> 53;
        }
        *level_1_pte = paging_make_pte_Sv39(pte_address, flags);
    }
    else
    {
        if (to_virt)
        {
            root_pte = (page_table_entry *)PAGING_Phys_to_Virt((uint64_t)root_pte)
        }
        if (*root_pte == 0)
        {
            return VERA_ERR_INVAL;
        }
        page_table *level_1_table = (page_table *)((*root_pte >> 10) << 12);
        page_table_entry *level_1_pte = &level_1_table[vpn_1];
        if (to_virt)
        {
            level_1_pte = (page_table_entry *)PAGING_Phys_to_Virt((uint64_t)level_1_pte)
        }
        if (*level_1_pte == 0)
        {
            return VERA_ERR_INVAL;
        }
        page_table *leaf_table = (page_table *)((*level_1_pte >> 10) << 12);
        page_table_entry *leaf_pte = &leaf_table[vpn_0];
        if (to_virt)
        {
            leaf_pte = (page_table_entry *)PAGING_Phys_to_Virt((uint64_t)leaf_pte)
        }
        if (*leaf_pte == 0)
        {
            return VERA_ERR_INVAL;
        }
        uint64_t pte_address = 0;
        if (new_p_address != 0)
        {
            pte_address = new_p_address;
        }
        else
        {
            pte_address = (*leaf_pte >> 10) << 12;
        }
        if (flags == 0)
        {
            flags = (*leaf_pte << 53) >> 53;
        }
        *leaf_pte = paging_make_pte_Sv39(pte_address, flags);
    }
#if INCLUDE_DEBUG
    vera_uart_print("Leave paging_edit_page_Sv39\n");
#endif
    return VERA_OK;
}

// Helper Function


/*
Erstellt den PTE gerecht für den Standart Sv39

params:
- (const) phys_address -> Die addresse auf die der pte zeigt
- (const) flags -> Die flags die der pte hat

return:
- der fertige pte
*/
static inline uint64_t paging_make_pte_Sv39(uint64_t phys_address, uint64_t flags)
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
uint64_t vera_paging_get_leaf_pages_amount(uint64_t size_of_region)
{
    uint64_t padding_pages = size_of_region % Page_Size;
    uint64_t pages = size_of_region / Page_Size;
    if (padding_pages)
    {
        ++pages;
    }
    return pages;
}

static uint64_t vera_paging_get_level1_pages_amount(uint64_t size_of_region)
{
    uint64_t pages = size_of_region / To_MiB(2);
    return pages;
}

static uint64_t vera_paging_get_level2_pages_amount(uint64_t size_of_region)
{
    uint64_t pages = size_of_region / To_GiB(1);
    return pages;
}
