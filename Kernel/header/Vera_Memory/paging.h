/* header/Vera_Memory/paging.h */
#pragma once

#include "../Vera_Utils/utils.h"




typedef enum {
    paging_Valid = 1,   // V
    paging_Read = 2,    // R
    paging_Write = 4,   // W
    paging_Execute = 8,  // X
    paging_U_Mode = 16,  // U -> Darf U Mode zugreifen
    paging_Global = 32,  // G -> Ist für jede ASID zugänglich
    paging_Accessed = 64, // A -> Wenn nicht aktiv, gibt Page Fault wenn Accessed
    paging_Dirty = 128, // D -> Wenn nicht gesetzt, gibt Page Fault wenn geschrieben wird.
    paging_is_pte2 = 256,
    paging_is_pte1 = 512,
}paging_flags;

typedef struct {
    page_table* root_table;
    uint8_t* next_free_table;
}paging_table_container;

vera_state paging_map_pages_Sv39(paging_table_container* const root_table , phys_address p_address, virt_address v_address_internal, uint64_t needed_pages, paging_flags flags, bool is_Global, bool to_virt);

vera_state paging_edit_page_Sv39(paging_table_container* const root_table, virt_address v_address, phys_address new_p_address, paging_flags flags, bool to_virt);

uint64_t vera_paging_get_leaf_pages_amount(uint64_t size_of_region);

extern void activatePaging(uint64_t* root_table, uint16_t asid);

extern void switchPageTable(uint64_t* root_table, uint16_t asid);

extern void flushTLB_Cache(void);