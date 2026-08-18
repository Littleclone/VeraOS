/* header/mem_controller.h */
#pragma once

#include "dtb_parser.h"
#include "../../Kernel/header/kernel.h"
#include "../../Kernel/header/Vera_Utils/general_define.h"


typedef enum {
    mem_invalid_lily = 0,
    mem_free_memory_dirty_lily = 1,
    mem_free_memory_clean_lily = 2,

    mem_reserved_kernel_lily = 3,
    mem_reserved_kernel_heap_lily = 4,
    mem_reserved_kernel_paging_table_lily = 5,
    
    mem_reserved_unknown_lily = 6,
    mem_reserved_kernel_info_lily = 7,
    mem_reserved_dtb = 8,


}mem_mem_map_lily;

typedef struct {
    dtb_tree_lily* tree;
    dtb_Memory_Node* mem_nodes;
    dtb_reserved_entry* reserved_nodes;
    uint16_t mem_node_counter;
}mem_convert_infos;

typedef struct {
    phys_address address;                   // the starting Physical address
    size_t size;                            // the size in bytes
    virt_address v_address_internal;        // the Starting Virtuell Address
    virt_address v_address_user;            // the starting virtuell address for userland
    I_PID process_ID;                       // the intern process ID who owns this Memory area (Combine with mem_state), default Kernel = 0
    mem_mem_map_lily mem_state;      // the state of the memory-area
}mem_node_lily;

kernel_boot_info* mem_convert_memory_nodes(mem_convert_infos info_block, kernel_area* kernel_region);