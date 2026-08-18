/* header/dtb_parser.h*/

#pragma once

#include "../../Kernel/header/kernel.h"

#define Max_Memory_Reservation_Blocks 16

typedef enum {
    fdt_begin_node_lily = 1,
    fdt_end_node_lily = 2,
    fdt_prop_lily = 3,
    fdt_nop_lily = 4,
    fdt_end_lily = 9,
}lily_fdt_token;

typedef enum {
    dtb_free_Memory = 0,
    dtb_reserved_unknown = 1,
    dtb_reserved_kernel = 2,
    
}dtb_MemMap_State;

typedef struct {
    uint32_t magic;             // Must contain the value "0xd00dfeed" in big Endian
    uint32_t totalsize;         // The Totalsize of the DTB Struct in bytes
    uint32_t off_dt_struct;     // Offset in Bytes of the Structure Block from the beginning of the header.
    uint32_t off_dt_strings;    // Offset in Bytes of the String block from the beginning of the Header
    uint32_t off_mem_rsvmap;    // Offset in Bytes of the Memory Reservation block from the beginning of the Header
    uint32_t version;           // Has the Version Number of the Devicetree (here 17)           
    uint32_t last_comp_version; // The lowest version of the Device tree strucutre?
    uint32_t boot_cpuid_phys;   // This field shall contain the physical ID of the system’s boot CPU. It shall be identical to the physical ID given in the reg property of that CPU node within the devicetree
    uint32_t size_dt_strings;   // The lenght in bytes of the Strings block of the devictree blob.
    uint32_t size_dt_struct;    // The lenght in bytes of the structure block of the section of the devicetree blob.
}dtb_header_lily;                    // Flattened Devicetree Header. Alles Big Endian

typedef struct  {
uint64_t address;
uint64_t size;
}dtb_reserved_entry;         // Auch für Allokator needed

typedef struct dtb_Memory_Node{
    uint64_t address;
    uint64_t size;
    uint32_t* reg;              // needs to be Endianess Swapped
    uint32_t length_of_property;    // For Reg
    dtb_MemMap_State mem_map_state;
}dtb_Memory_Node;     // Die Basic Node mit der der Allokator Arbeitet

typedef struct {
    dtb_header_lily header;
    dtb_reserved_entry* memory_block;             // If on the end of List the Entry has on Both uint64_t a "0"
    uint8_t* struct_block;
    uint8_t* string_block;
}dtb_tree_lily;  // dtb

kernel_boot_info* dtb_setup_memory(dtb_tree_lily* dtb, kernel_area* kernel_region);