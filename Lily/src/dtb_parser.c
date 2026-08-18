/* src/dtb_parser.c */
#include "../header/dtb_parser.h"
#include "../header/mem_controller.h"
#include "../header/lily_extension.h"

// Funktionen
static kernel_boot_info *dtb_get_memory_nodes(dtb_tree_lily *dtb, dtb_reserved_entry *memory_block, uint32_t *node_struct, uint8_t *string_struct, kernel_area *kernel_region);

// Definen
#define root_address_cell_default 2
#define root_size_cell_default 1

#define Byte_Size_Per_Token 4
#define Uint32_t_bytes Byte_Size_Per_Token

/*
    Function that prepares to Search in the DTB for Memory Nodes

    params:
    - (ptr) dtb -> Pointer to the Device Tree Bloob
    - (ptr) kernel_region -> To give it to the Memory Function

    return:
    - (ptr) kernel_boot_info -> Holds Informations for the Kernel
    - NULL -> Error
*/
kernel_boot_info *dtb_setup_memory(dtb_tree_lily *dtb, kernel_area *kernel_region)
{

    #if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter dtb_setup_memory\n");
    #endif

    if (dtb == NULL || kernel_region == NULL)
    {
        #if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
        #else
        lily_panic_no_write(K_ERR_NULL_PTR);
        #endif
    }
    // Check the Magic number of the Header
    uint32_t magic = lily_utils_swap_endian32(dtb->header.magic);
    if (magic != 0xd00dfeed)
    {
        #if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_INVAL);
        #else
        lily_panic_no_write(VERA_ERR_INVAL);
        #endif
    }
    uint32_t version = lily_utils_swap_endian32(dtb->header.version);
    if (version != 17)
    {
        #if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_INVAL);
        #else
        lily_panic_no_write(VERA_ERR_INVAL);
        #endif
    }

    // Parse Basic Device Tree Blob (dtb) for the Allokator (Memory Only)
    // Init Memory Block
    uint8_t *temp_ptr = (uint8_t *)dtb;
    temp_ptr += lily_utils_swap_endian32(dtb->header.off_mem_rsvmap);
    dtb_reserved_entry *memory_block = (dtb_reserved_entry *)temp_ptr;

    // Init Struct Block
    temp_ptr = (uint8_t *)dtb;
    temp_ptr += lily_utils_swap_endian32(dtb->header.off_dt_struct);
    uint32_t *node_struct = (uint32_t *)temp_ptr;

    // Init String Block
    temp_ptr = (uint8_t *)dtb;
    temp_ptr += lily_utils_swap_endian32(dtb->header.off_dt_strings);
    uint8_t *string_struct = temp_ptr;

    // Holen uns Basic Memory Nodes
    kernel_boot_info* boot_info = dtb_get_memory_nodes(dtb, memory_block, node_struct, string_struct, kernel_region);
    #if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave dtb_setup_memory\n");
    #endif
    return boot_info;
}

/*
Searches for the Memory Nodes in the Device Tree

    params:
    - (ptr) dtb -> The Pointer to the Device Tree Blob
    - (ptr) memory_block -> The Pointer to the Reserved DTB Memory Entries (Depracet if i remember right)
    - (ptr) node_struct -> The Pointer where the Device Tree (FDT) lays.
    - (ptr) string_struct -> Points to the strings
    - (ptr) kernel_region -> Give it to the Memmory Function

    return:
    - (ptr) kernel_boot_info -> Holds Informations for the Kernel
    - NULL -> Error
*/
static kernel_boot_info *dtb_get_memory_nodes(dtb_tree_lily *dtb, dtb_reserved_entry *memory_block, uint32_t *node_struct, uint8_t *string_struct, kernel_area *kernel_region)
{

    #if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter dtb_get_memory_nodes\n");
    #endif
    if (dtb == NULL || memory_block == NULL || node_struct == NULL || string_struct == NULL || kernel_region == NULL) {
        #if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
        #else
        lily_panic_no_write(K_ERR_NULL_PTR);
        #endif
    } 

    // Init Stack_Array für Nodes die der Allokator braucht
    dtb_reserved_entry entry[Max_Memory_Reservation_Blocks];
    dtb_Memory_Node memory_nodes[Max_Memory_Nodes];
    uint8_t generic_counter = 0;

    // Reserved Memory Blocks structs
    while (1)
    {
        uint64_t address = lily_utils_swap_endian64(memory_block->address);
        uint64_t size = lily_utils_swap_endian64(memory_block->size);
        entry[generic_counter].address = address;
        entry[generic_counter].size = size;
        ++generic_counter;
        if (address == 0 && size == 0)
        {
            break;
        }
        else if (generic_counter >= Max_Memory_Reservation_Blocks)
        {
#if INCLUDE_DEBUG && INCLUDE_QEMU
            lily_panic(VERA_OVERFLOW);
#else
            lily_panic_no_write(VERA_OVERFLOW);
#endif
        }
    }

    // Braucht system um zwischen dem und in Nodes gesetzen zu unterscheiden, aber auch wieder zurück zu root.
    uint8_t current_address_cells = root_address_cell_default;
    uint8_t current_size_cells = root_size_cell_default;
    uint8_t root_address_cell = root_address_cell_default;
    uint8_t root_size_cells = root_size_cell_default;

    bool is_memory = false;
    bool is_reserved_memory = false;
    uint16_t depth = 0;
    generic_counter = 0;

    // begin the search for the Memory Nodes in the Device tree
    // TODO: HART Test if we support it!
    while (1)
    {
        lily_fdt_token token = lily_utils_swap_endian32(*node_struct);
        ++node_struct;
        // Begin from a Node
        if (token == fdt_begin_node_lily)
        {
            ++depth;
            char *node_name = (char *)node_struct;
            uint16_t lenght = (uint16_t)lily_utils_str_len(node_name);
            // Move Node Struct to the end of the String and align
            node_struct = (uint32_t *)(((uint8_t *)node_struct) + lenght);
            //++node_struct;
            node_struct = (uint32_t *)lily_utils_align((uint64_t)node_struct, Byte_Size_Per_Token);
#if INCLUDE_DEBUG && INCLUDE_QEMU
            lily_uart_printf("Node: %s\n", node_name);
#endif
            // if the node is Memory
            if (lily_utils_str_has(node_name, "memory"))
            {
                if (generic_counter >= Max_Memory_Nodes)
                {
#if INCLUDE_DEBUG && INCLUDE_QEMU
                    lily_panic(VERA_OVERFLOW);
#else
                    lily_panic_no_write(VERA_OVERFLOW);
#endif
                }
                // If node is reserved Memory
                if (lily_utils_str_has(node_name, "reserved"))
                {
                    is_reserved_memory = true;
                }
                else
                {
                    is_memory = true;
                }
                memory_nodes[generic_counter].address = current_address_cells;
                memory_nodes[generic_counter].size = current_size_cells;
            }
        }
        // Property of a Node
        else if (token == fdt_prop_lily)
        {
            uint32_t length = lily_utils_swap_endian32(*node_struct);
            ++node_struct;
            char *name_of_property = ((char *)string_struct) + lily_utils_swap_endian32(*node_struct);
            ++node_struct;

            if (is_memory || is_reserved_memory || depth == 1)
            {
                if (generic_counter >= Max_Memory_Nodes)
                {
#if INCLUDE_DEBUG && INCLUDE_QEMU
                    lily_panic(VERA_OVERFLOW);
#else
                    lily_panic_no_write(VERA_OVERFLOW);
#endif
                }
#if INCLUDE_DEBUG && INCLUDE_QEMU
                lily_uart_printf("Property_Name: %s\n", name_of_property);
#endif
                if (lily_utils_str_cmp(name_of_property, "#address-cells"))
                {
                    uint8_t address_cell = (uint8_t)lily_utils_swap_endian32(*node_struct);
                    if (depth != 1)
                    {
                        memory_nodes[generic_counter].address = address_cell;
                        current_address_cells = address_cell;
                    }
                    // If we currently set the Root Node
                    else
                    {
                        root_address_cell = address_cell;
                    }
                }
                else if (lily_utils_str_cmp(name_of_property, "#size-cells"))
                {
                    uint8_t size_cell = (uint8_t)lily_utils_swap_endian32(*node_struct);
                    if (depth != 1)
                    {
                        memory_nodes[generic_counter].size = size_cell;
                        current_size_cells = size_cell;
                    }
                    // If we currently set the Root Node
                    else
                    {
                        root_size_cells = size_cell;
                    }
                }
                else if (lily_utils_str_cmp(name_of_property, "reg"))
                {
                    uint32_t *regs = node_struct;
                    memory_nodes[generic_counter].reg = regs;
                    memory_nodes[generic_counter].length_of_property = length;
                }

                if (is_reserved_memory)
                {
                    memory_nodes[generic_counter].mem_map_state = dtb_reserved_unknown;
                }
                else
                {
                    memory_nodes[generic_counter].mem_map_state = dtb_free_Memory;
                }
            }
            node_struct = (uint32_t *)(((uint8_t *)node_struct) + length);
            node_struct = (uint32_t *)lily_utils_align((uint64_t)node_struct, Byte_Size_Per_Token);
        }
        // If nothing
        else if (token == fdt_nop_lily || token == 0)
        {
            continue;
        }
        // If nodes end
        else if (token == fdt_end_node_lily)
        {
            --depth;
            if (1 == depth)
            {
                is_memory = false;
                is_reserved_memory = false;
                current_address_cells = root_address_cell;
                current_size_cells = root_size_cells;
            }
            if (is_memory || is_reserved_memory)
            {
                ++generic_counter;
                memory_nodes[generic_counter].address = current_address_cells;
                memory_nodes[generic_counter].size = current_size_cells;
            }
        }
        // Device Tree ends
        else if (token == fdt_end_lily)
        {
            break;
        }
        // Undefinde Token
        else
        {
#if INCLUDE_DEBUG && INCLUDE_QEMU
            lily_panic(VERA_ERR_INVAL);
#else
            lily_panic_no_write(VERA_ERR_INVAL);
#endif
        }
    }
#if INCLUDE_DEBUG && INCLUDE_QEMU
    uint16_t i = 0;
    dtb_Memory_Node *nodes = memory_nodes;
    while (i <= generic_counter)
    {
        lily_uart_printf("Address: %i, Size: %i, State: %i\nReg<", memory_nodes[i].address, memory_nodes[i].size, memory_nodes[i].mem_map_state);

        for (uint32_t j = 0; j < (memory_nodes[i].length_of_property / Uint32_t_bytes); ++j)
        {
            lily_uart_printf("%ixX ", lily_utils_swap_endian32(memory_nodes[i].reg[j]));
        }
        lily_uart_print(">\n");
        ++i;
    }
#endif
    // Go to Memory Controller Lily
    mem_convert_infos convert_informations;
    convert_informations.mem_nodes = memory_nodes;
    convert_informations.reserved_nodes = entry;
    convert_informations.mem_node_counter = generic_counter;
    convert_informations.tree = dtb;

    kernel_boot_info* boot_info = mem_convert_memory_nodes(convert_informations, kernel_region);
    #if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave dtb_get_memory_nodes\n");
    #endif
    return boot_info;
}
