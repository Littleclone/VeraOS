/* header/Vera_Device_Driver/dtb_parser.h */
#pragma once

#include "../Vera_Utils/utils.h"

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
}dtb_header;   


typedef struct {
    dtb_header header;
    uint8_t* struct_block;
    uint8_t* string_block;
}dtb_tree;  // dtb


vera_state dtb_init(uint64_t hart_id, dtb_tree* tree);