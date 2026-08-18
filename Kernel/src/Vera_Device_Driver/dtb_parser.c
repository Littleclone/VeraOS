/* src/Vera_Device_Driver/dtb_parser.c */
#include "../../header/Vera_Device_Driver/dtb_parser.h"
#include "../../header/Vera_UART/uart.h"
#include "../../header/Vera_Memory/allocator.h"
#include "../../header/Vera_Memory/mem_controller.h"
#include "../../header/Vera_Device_Driver/driver_support.h"
#include "../../header/Vera_Device_Driver/PCIe.h"
#include "../../header/Vera_Interrupt/IMSIC.h"
#include "../../header/Vera_Interrupt/APLIC.h"
#include "../header/hart.h"

typedef struct
{
    uint32_t *ptr;
    uint64_t lenght_in_bytes;
} Node_Property;

typedef struct
{
    char *strings;
    uint64_t elements;
} String_List;

typedef struct Base_Node
{
    char *node_title; // The Node Name (Myself added, instead of "Node_Name" that would be depracated)
    String_List compatible;

    uint32_t *raw_dtb_node; // Der Pointer zur Raw Node im Device Tree an dem wir interessiert sind

    struct Base_Node *next;   // The next Node comes after it
    struct Base_Node *parent; // The Parent

    uint32_t address_cell; // Default bei Root, 2
    uint32_t size_cell;    // Default bei Root, 1
    uint32_t interrupt_cell;

} Base_Node;

typedef enum
{
    fdt_nothing = 0x0000,
    fdt_begin_node = 0x0001,
    fdt_end_node = 0x0002,
    fdt_prop = 0x0003,
    fdt_nop = 0x0004,
    fdt_end = 0x0009,
} device_tree_token;

#define Byte_Size_Per_Token 4
#define Uint32_t_bytes Byte_Size_Per_Token

static void dtb_parse_tree(uint32_t *node_struct, uint8_t *string_node, Base_Node *root);

static void dtb_check_drivers(Base_Node *root_node, uint8_t *string_node);

static Base_Node *dtb_get_next_wanted_node(char *node_title, Base_Node *current_node, bool contains_title);

static Node_Property *dtb_get_node_information(char *node_title, Base_Node *info_node, char *propery_wanted, uint8_t *string_node);

static Base_Node *P_root;

/*
Initializes to parse the Device Tree for the First time and also Checks if the Version and Boot Hart ID is correct.

params:
- hard_id -> The ID our Kernel Starts from, it checks if it matches the one in the DTB Header
- (ptr) tree -> The DTB_Tree, the Pointer got from OpenSBI -> Lily -> Kernel (To us)

return:
- VERA_ERR_NULL_PTR -> Tree is NULL or k_malloc didn't worked / we didn't found anything in the Device Tree
- VERA_ERR_INVAL -> Hart_ID is not the one we should bootet or the Magic doesn't pass or the Version is not our Supportet version 17
- VERA_OK -> Everything worked out
*/
vera_state dtb_init(uint64_t hart_id, dtb_tree *tree)
{

#if INCLUDE_DEBUG
    vera_uart_print("Enter dtb_init\n");
#endif
    dtb_header header = tree->header;

    if (tree == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }

    if (vera_utils_swap_endian32(header.boot_cpuid_phys) != hart_id || vera_utils_swap_endian32(header.magic) != 0xd00dfeed || vera_utils_swap_endian32(tree->header.version) != 17)
    {
        return VERA_ERR_INVAL;
    }

    uint8_t *temp_ptr = (uint8_t *)tree;
    temp_ptr += vera_utils_swap_endian32(tree->header.off_dt_struct);
    uint32_t *struct_node = (uint32_t *)temp_ptr;
    temp_ptr = (uint8_t *)tree;
    temp_ptr += vera_utils_swap_endian32(tree->header.off_dt_strings);
    uint8_t *string_node = temp_ptr;

    // The Root DTB Node
    Base_Node *root_node = (Base_Node *)k_malloc(sizeof(Base_Node));
    if (root_node == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
    // Init the Root Node:
    root_node->node_title = "/";
    root_node->compatible.strings = NULL;
    root_node->raw_dtb_node = NULL;
    root_node->next = NULL;
    root_node->parent = NULL;
    root_node->compatible.elements = 0;

    root_node->address_cell = 2;
    root_node->size_cell = 1;
    root_node->interrupt_cell = 0;

    // Start Dynamic Parsing
    dtb_parse_tree(struct_node, string_node, root_node);
    // End of Dynamic Parsing, check if everything is fine.
    if (root_node->next == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }

    dtb_check_drivers(root_node, string_node);
    P_root = root_node;

#if INCLUDE_DEBUG
    vera_uart_print("Leave dtb_init\n");
#endif
    return VERA_OK;
}

// Dynamic Parsing

/*
Parses the Tree and find every Node and collects them in Base Node.

params:
- (ptr) node_struct -> the node where the Device Tree Nodes are in Memory
- (ptr) string_node -> The node where pointing to the place in Memory where the Strings are
- (ptr) root -> The Base Root Node that is our Root
*/
static void dtb_parse_tree(uint32_t *node_struct, uint8_t *string_node, Base_Node *root)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter dtb_parser_tree\n");
#endif
    Base_Node *cur_node = root;
    Base_Node *next_node = root;
    uint32_t depth = 0;
    bool is_root = true;
#if INCLUDE_DEBUG
    vera_uart_print("\n\n");
#endif
    while (1)
    {
        device_tree_token token = (vera_utils_swap_endian32(*node_struct));
        ++node_struct;
        if (token == fdt_begin_node)
        {
            char *node_name = (char *)node_struct;
            ++node_struct;
            node_struct = (uint32_t *)vera_utils_align((uint64_t)node_struct, Byte_Size_Per_Token);
#if INCLUDE_DEBUG
            for (uint32_t i = 0; i < depth; ++i)
            {
                vera_uart_print("---");
            }
            vera_uart_printf("Node_Name: %s\n", node_name);
#endif

            if (!is_root)
            {
                Base_Node *new_child = k_malloc(sizeof(Base_Node));
                if (new_child == NULL)
                {
                    vera_err_boot_panic(VERA_ERR_NOMEM);
                }
                new_child->node_title = node_name;
                new_child->parent = cur_node;
                new_child->address_cell = cur_node->address_cell;
                new_child->size_cell = cur_node->size_cell;
                new_child->compatible.strings = NULL;
                new_child->compatible.elements = 0;
                new_child->next = NULL;

                next_node->next = new_child;

                cur_node = new_child;
                next_node = new_child;
            }
            else
            {
                is_root = false;
            }
            cur_node->raw_dtb_node = (node_struct - 2);

            ++depth;
        }
        else if (token == fdt_prop)
        {
            uint32_t length_of_property = vera_utils_swap_endian32(*node_struct);
            ++node_struct;
            char *name_off = ((char *)string_node) + vera_utils_swap_endian32(*node_struct);
            ++node_struct;
#if INCLUDE_DEBUG
            for (uint32_t i = 0; i < depth; ++i)
            {
                vera_uart_print("---");
            }
            vera_uart_printf("Property Name: %s\n", name_off);

            // Weiteres auslesen um mir dinge anzuschauen, das nur in Debug ausgegeben werden sollen.

            if (vera_utils_str_cmp(name_off, "#address-cells"))
            {
                uint32_t address_cell = vera_utils_swap_endian32(*node_struct);
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Address-cell: <%i>\n", address_cell);
                cur_node->address_cell = address_cell;
            }
            else if (vera_utils_str_cmp(name_off, "#size-cells"))
            {
                uint32_t size_cell = vera_utils_swap_endian32(*node_struct);
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Size-cell: <%i>\n", size_cell);
                cur_node->size_cell = size_cell;
            }
            else if (vera_utils_str_cmp(name_off, "#interrupt-cells"))
            {
                uint32_t interrupt_cell = vera_utils_swap_endian32(*node_struct);
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Interrupt-cell: <%i>\n", interrupt_cell);
                cur_node->interrupt_cell = interrupt_cell;
            }
            else if (vera_utils_str_cmp(name_off, "model"))
            {
                char *model = (char *)node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Model: %s\n", model);
            }
            else if (vera_utils_str_cmp(name_off, "ranges"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                if (length_of_property == fdt_nothing)
                {
                    vera_uart_print("Ranges: <Empty>\n");
                    goto End;
                }
                vera_uart_print("Ranges: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node));
                    ++temp_node;
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "reg"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Reg: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "value"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Value: <%ixX>\n", vera_utils_swap_endian32(*node_struct));
            }
            else if (vera_utils_str_cmp(name_off, "offset"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Offset: <%ixX>\n", vera_utils_swap_endian32(*node_struct));
            }
            else if (vera_utils_str_cmp(name_off, "regmap"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Regmap: <%ixX>\n", vera_utils_swap_endian32(*node_struct));
            }
            else if (vera_utils_str_cmp(name_off, "phandle"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Phandle: <%ixX>\n", vera_utils_swap_endian32(*node_struct));
            }
            else if (vera_utils_str_cmp(name_off, "timebase-frequency"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Timebase-Freqzency: <%ixX>\n", vera_utils_swap_endian32(*node_struct));
            }
            else if (vera_utils_str_cmp(name_off, "device_type"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Device_Type: %s\n", node_struct);
            }
            else if (vera_utils_str_cmp(name_off, "status"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Status: %s\n", node_struct);
            }
            else if (vera_utils_str_cmp(name_off, "virtual-reg"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("irtual-Reg: %ixX\n", vera_utils_swap_endian32(*node_struct));
            }
            else if (vera_utils_str_cmp(name_off, "interrupts"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Interrupts: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "interrupt-parent"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_printf("Interrupts-Partent: %ixX\n", vera_utils_swap_endian32(*node_struct));
            }
            else if (vera_utils_str_cmp(name_off, "interrupts-extended"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Interrupts-Extended: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "interrupt-map"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Interrupt-Map: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "interrupt-map-mask"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Interrupt-Map-Mask: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "bus-range"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Bus-Range: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "linux,pci-domain"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Linux,PCI_Domain: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "riscv,ndev"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("riscv,ndev: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "clock-frequency"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Clock-Frequency: <");
                if (length_of_property == 4)
                {
                    vera_uart_printf("%ixX>\n", vera_utils_swap_endian32(*node_struct));
                }
                else if (length_of_property == 8)
                {
                    vera_uart_printf("%ilxX>\n", vera_utils_swap_endian64(*node_struct));
                }
                else
                {
                    vera_err_boot_panic(VERA_ERR_NOSUP);
                }
            }
            else if (vera_utils_str_cmp(name_off, "stdout-path"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                char *string = (char *)node_struct;
                vera_uart_printf("stdout-path: %s\n", string);
            }
            else if (vera_utils_str_cmp(name_off, "stdin-path"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                char *string = (char *)node_struct;
                vera_uart_printf("stdout-path: %s\n", string);
            }
            else if (vera_utils_str_cmp(name_off, "rng-seed"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("rng-seed: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "aliases"))
            {
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                char *string = (char *)node_struct;
                vera_uart_printf("Aliases: %s\n", string);
            }
            // In the Qemu VIRT Dump
            else if (vera_utils_str_cmp(name_off, "riscv,event-to-mhpmcounters"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("riscv,event-to-mhpmcounters: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "riscv,cbop-block-size"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("riscv,cbop-block-size: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
            else if (vera_utils_str_cmp(name_off, "riscv,isa-extensions"))
            {
                char *compatible = (char *)node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                uint32_t temp_lenght = length_of_property;
                char *temp_compatible = compatible;
                vera_uart_print("riscv,isa-extensions: ");
                while (temp_lenght)
                {
                    vera_uart_printf("%s ", temp_compatible);
                    uint32_t str_len = (uint32_t)vera_utils_str_len(temp_compatible) + 1;
                    temp_compatible += str_len;
                    temp_lenght -= str_len;
                }
                vera_uart_print(" \n");
            }
            else if (vera_utils_str_cmp(name_off, "riscv,isa-base"))
            {
                char *compatible = (char *)node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                uint32_t temp_lenght = length_of_property;
                char *temp_compatible = compatible;
                vera_uart_print("riscv,isa-base: ");
                while (temp_lenght)
                {
                    vera_uart_printf("%s ", temp_compatible);
                    uint32_t str_len = (uint32_t)vera_utils_str_len(temp_compatible) + 1;
                    temp_compatible += str_len;
                    temp_lenght -= str_len;
                }
                vera_uart_print(" \n");
            }
            else if (vera_utils_str_cmp(name_off, "riscv,isa"))
            {
                char *compatible = (char *)node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                uint32_t temp_lenght = length_of_property;
                char *temp_compatible = compatible;
                vera_uart_print("riscv,isa: ");
                while (temp_lenght)
                {
                    vera_uart_printf("%s ", temp_compatible);
                    uint32_t str_len = (uint32_t)vera_utils_str_len(temp_compatible) + 1;
                    temp_compatible += str_len;
                    temp_lenght -= str_len;
                }
                vera_uart_print(" \n");
            }
            else if (vera_utils_str_cmp(name_off, "mmu-type"))
            {
                char *compatible = (char *)node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                uint32_t temp_lenght = length_of_property;
                char *temp_compatible = compatible;
                vera_uart_print("MMU-Type: ");
                while (temp_lenght)
                {
                    vera_uart_printf("%s ", temp_compatible);
                    uint32_t str_len = (uint32_t)vera_utils_str_len(temp_compatible) + 1;
                    temp_compatible += str_len;
                    temp_lenght -= str_len;
                }
                vera_uart_print(" \n");
            }
            else if (vera_utils_str_cmp(name_off, "bank-width"))
            {
                uint32_t *temp_node = node_struct;
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                vera_uart_print("Bank Width: <");
                for (uint32_t i = 0; i < (length_of_property / Uint32_t_bytes); ++i)
                {
                    vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node++));
                }
                vera_uart_print(">\n");
            }
        End:
#endif
            if (vera_utils_str_cmp(name_off, "compatible"))
            {
                char *compatible = (char *)node_struct;
#if INCLUDE_DEBUG
                for (uint32_t i = 0; i < (depth + 1); ++i)
                {
                    vera_uart_print("---");
                }
                uint32_t temp_lenght = length_of_property;
                char *temp_compatible = compatible;
                vera_uart_print("Compatible: ");
                while (temp_lenght)
                {
                    vera_uart_printf("%s ", temp_compatible);
                    uint32_t str_len = (uint32_t)vera_utils_str_len(temp_compatible) + 1;
                    temp_compatible += str_len;
                    temp_lenght -= str_len;
                }
                vera_uart_print(" \n");
#endif
                cur_node->compatible.strings = compatible;
                cur_node->compatible.elements = length_of_property;
            }
            // Nächster Valid token
            uint8_t *temp_ptr = (uint8_t *)node_struct;
            temp_ptr += length_of_property;
            node_struct = (uint32_t *)temp_ptr;
            node_struct = (uint32_t *)vera_utils_align((uint64_t)node_struct, Byte_Size_Per_Token);
        }
        else if (token == fdt_nop || token == fdt_nothing)
        {
            continue;
        }
        else if (token == fdt_end_node)
        {
            --depth;
#if INCLUDE_DEBUG
            for (uint32_t i = 0; i < depth; ++i)
            {
                vera_uart_print("---");
            }
            vera_uart_printf("End Node, New Depth: %i\n", depth);
#endif
            cur_node = cur_node->parent;
        }
        else if (token == fdt_end)
        {
#if INCLUDE_DEBUG
            vera_uart_printf("End! %i\n", depth);
#endif
            break;
        }
    }
#if INCLUDE_DEBUG
    vera_uart_print("Leave dtb_parser_tree\n");
#endif
}

// Check for Driver Initialising

/*
Will Check in the DTB if we find Drivers for the Nodes and initilize them.

params:
- (ptr) root_node -> our Root Node from the Device tree
- (ptr) string_node -> Our Node Pointing to the Strings in Memory
*/
static void dtb_check_drivers(Base_Node *root_node, uint8_t *string_node)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter dtb_check_drivers\n");
#endif
    // Take the root node and itterate through it.
    Base_Node *cur_node = root_node;
    cur_node = cur_node->next;
    // Go Through all nodes to check for drivers until we are done.
    while (cur_node != NULL)
    {

        if (vera_utils_str_cmp(cur_node->node_title, "cpus"))
        {
            // Init CPU parsing
            Base_Node *cpu_node = cur_node;
            vera_utils_safe_array cpu_saved;
            cpu_saved.ptr_array = (void *)k_malloc(sizeof(hart_info_struct_deep) * 20);
            if (cpu_saved.ptr_array == NULL)
            {
#if INCLUDE_DEBUG
                vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
            }
            cpu_saved.counter = 0;
            cpu_saved.max_elements = 20;
            hart_info_struct_deep *harts = (hart_info_struct_deep *)cpu_saved.ptr_array;

            // Locale variables for Global stats from CPU's

            char *g_status = NULL;
            char *g_mmu_type = NULL;
            char *g_isa = NULL;
            uint32_t g_cbop_block = UINT32_MAX;
            uint32_t g_cboz_block = UINT32_MAX;
            uint32_t g_cbom_block = UINT32_MAX;
            uint32_t g_lenght = UINT32_MAX;
            uint64_t g_timebase = UINT64_MAX;
            uint64_t g_clock = UINT64_MAX;

            Node_Property *global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "status", string_node);
            if (global_property != NULL)
            {
                g_status = (char *)global_property->ptr;
                k_free(global_property);
            }
            global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "mmu-type", string_node);
            if (global_property != NULL)
            {
                g_mmu_type = (char *)global_property->ptr;
                k_free(global_property);
            }
            global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,isa-extensions", string_node);
            if (global_property != NULL)
            {
                g_isa = (char *)global_property->ptr;
                g_lenght = global_property->lenght_in_bytes;
                k_free(global_property);
            }
            global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,cbop-block-size", string_node);
            if (global_property != NULL)
            {
                g_cbop_block = vera_utils_swap_endian32(*global_property->ptr);
                k_free(global_property);
            }
            global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,cboz-block-size", string_node);
            if (global_property != NULL)
            {
                g_cboz_block = vera_utils_swap_endian32(*global_property->ptr);
                k_free(global_property);
            }
            global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,cbom-block-size", string_node);
            if (global_property != NULL)
            {
                g_cbom_block = vera_utils_swap_endian32(*global_property->ptr);
                k_free(global_property);
            }
            global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "timebase-frequency", string_node);
            if (global_property != NULL)
            {
                g_timebase = vera_utils_swap_endian32(*global_property->ptr);
                ++global_property;
                if (global_property->lenght_in_bytes >= 8)
                {
                    g_timebase <<= 32;
                    g_timebase |= vera_utils_swap_endian32(*global_property->ptr);
                }
                k_free(global_property);
            }
            global_property = dtb_get_node_information(cpu_node->node_title, cpu_node, "clock-frequency", string_node);
            if (global_property != NULL)
            {
                g_clock = vera_utils_swap_endian32(*global_property->ptr);
                ++global_property;
                if (global_property->lenght_in_bytes >= 8)
                {
                    g_clock <<= 32;
                    g_clock |= vera_utils_swap_endian32(*global_property->ptr);
                }
                k_free(global_property);
            }
            global_property = NULL;
            cpu_node = dtb_get_next_wanted_node("cpu", cpu_node, true);

            // Enter Parsing
            while (cpu_node != NULL && vera_utils_str_has(cpu_node->node_title, "cpu"))
            {
                if (vera_utils_str_cmp(cpu_node->node_title, "cpu-map"))
                {
                    cpu_node = dtb_get_next_wanted_node("cpu", cpu_node, true);
                    continue;
                }
                hart_info_struct_deep *hart = &harts[cpu_saved.counter];

                Node_Property *property = dtb_get_node_information(cpu_node->node_title, cpu_node, "reg", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }
                hart->hart_id = vera_utils_swap_endian32(*property->ptr);
                ++property;
                if (cur_node->address_cell >= 2 || property->lenght_in_bytes >= 8)
                {
                    hart->hart_id <<= 32;
                    hart->hart_id |= vera_utils_swap_endian32(*property->ptr);
                }
                k_free(property);
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "status", string_node);
                if (property == NULL)
                {
                    hart->status = g_status;
                }
                else
                {
                    hart->status = (char *)property->ptr;
                    k_free(property);
                }
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "mmu-type", string_node);
                if (property == NULL)
                {
                    hart->mmu_type = g_mmu_type;
                }
                else
                {
                    hart->mmu_type = (char *)property->ptr;
                    k_free(property);
                }
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,isa-extensions", string_node);
                if (property == NULL)
                {
                    hart->isa_string = g_isa;
                    hart_init_function_flags(hart, g_lenght);
                }
                else
                {
                    hart->isa_string = (char *)property->ptr;
                    hart_init_function_flags(hart, property->lenght_in_bytes);
                    k_free(property);
                }
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,cbop-block-size", string_node);
                if (property == NULL)
                {
                    hart->cbop_block_size = g_cbop_block;
                }
                else
                {
                    hart->cbop_block_size = vera_utils_swap_endian32(*property->ptr);
                    k_free(property);
                }
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,cboz-block-size", string_node);
                if (property == NULL)
                {
                    hart->cboz_block_size = g_cboz_block;
                }
                else
                {
                    hart->cboz_block_size = vera_utils_swap_endian32(*property->ptr);
                    k_free(property);
                }
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "riscv,cbom-block-size", string_node);
                if (property == NULL)
                {
                    hart->cbom_block_size = g_cbom_block;
                }
                else
                {
                    hart->cbom_block_size = vera_utils_swap_endian32(*property->ptr);
                    k_free(property);
                }
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "timebase-frequency", string_node);
                if (property == NULL)
                {
                    hart->timebase_frequency = g_timebase;
                }
                else
                {
                    hart->timebase_frequency = vera_utils_swap_endian32(*property->ptr);
                    ++property;
                    if (property->lenght_in_bytes >= 8)
                    {
                        hart->timebase_frequency <<= 32;
                        hart->timebase_frequency |= vera_utils_swap_endian32(*property->ptr);
                    }
                    k_free(property);
                }
                property = dtb_get_node_information(cpu_node->node_title, cpu_node, "clock-frequency", string_node);
                if (property == NULL)
                {
                    hart->timebase_frequency = g_clock;
                }
                else
                {
                    hart->clock_frequency = vera_utils_swap_endian32(*property->ptr);
                    ++property;
                    if (property->lenght_in_bytes >= 8)
                    {
                        hart->clock_frequency <<= 32;
                        hart->clock_frequency |= vera_utils_swap_endian32(*property->ptr);
                    }
                    k_free(property);
                }

                // Get Interrupt Controller of the CPU

                Base_Node *interrupt = dtb_get_next_wanted_node("interrupt-controller", cpu_node, false);
                if (interrupt == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }
                property = dtb_get_node_information(interrupt->node_title, interrupt, "clock-frequency", string_node);
                if (property == NULL)
                {
                    hart->intc_phandle = UINT32_MAX;
                }
                else
                {
                    hart->intc_phandle = vera_utils_swap_endian32(*property->ptr);
                    k_free(property);
                }

                // Get CPU next node
                ++cpu_saved.counter;
                if (cpu_saved.counter >= cpu_saved.max_elements)
                {
                    hart_info_struct_deep *temp_saved = k_malloc(sizeof(hart_info_struct_deep) * cpu_saved.counter + 10);
                    if (temp_saved == NULL)
                    {
#if INCLUDE_DEBUG
                        vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                        k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                    }
                    hart_info_struct_deep *hart_nodes = (hart_info_struct_deep *)cpu_saved.ptr_array;
                    cpu_saved.ptr_array = (void *)temp_saved;
                    for (uint16_t i = 0; i < cpu_saved.counter; ++i)
                    {
                        temp_saved[i].cbom_block_size = hart_nodes[i].cbom_block_size;
                        temp_saved[i].cbop_block_size = hart_nodes[i].cbop_block_size;
                        temp_saved[i].cboz_block_size = hart_nodes[i].cboz_block_size;
                        temp_saved[i].clock_frequency = hart_nodes[i].clock_frequency;
                        temp_saved[i].hart_id = hart_nodes[i].hart_id;
                        temp_saved[i].intc_phandle = hart_nodes[i].intc_phandle;
                        temp_saved[i].isa_string = hart_nodes[i].isa_string;
                        temp_saved[i].mmu_type = hart_nodes[i].mmu_type;
                        temp_saved[i].status = hart_nodes[i].status;
                        temp_saved[i].timebase_frequency = hart_nodes[i].timebase_frequency;
                    }
                    harts = (hart_info_struct_deep *)cpu_saved.ptr_array;
                    cpu_saved.max_elements = cpu_saved.counter + 10;
                }

                cpu_node = dtb_get_next_wanted_node("cpu", cpu_node, true);
            }

            // Leave Parsing

            k_init_harts(&cpu_saved);
        }

        char *temp_compatible = (char *)cur_node->compatible.strings;
        uint32_t temp_lenght = cur_node->compatible.elements;
        while (temp_lenght)
        {
            // UART
            if (vera_utils_str_cmp(temp_compatible, UART_16550))
            {
                // Get Node Information for 16550
                struct UART_16550_driver *node_info = k_malloc(sizeof(struct UART_16550_driver));
                if (node_info == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                node_info->address_cell = cur_node->address_cell;
                node_info->size_cell = cur_node->size_cell;

                // Get the Informations
                // Reg is for where the Region is and how big.
                Node_Property *property = dtb_get_node_information(temp_compatible, cur_node, "reg", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                node_info->reg = property->ptr;
                node_info->byte_lenght = property->lenght_in_bytes;
                // Give Information to init_Driver
                k_free(property);
                vera_uart_init_driver((void *)node_info, UART_16550_ID);
                k_free(node_info);
            }
            else if (vera_utils_str_cmp(temp_compatible, PCIe_Driver))
            {
            }
            else if (vera_utils_str_cmp(temp_compatible, APLIC_Risc_V))
            {
                // Check if its our S Mode Aplic, by looking at its IMSICs parent
                Node_Property *property = dtb_get_node_information(cur_node->node_title, cur_node, "msi-parent", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                // Check
                uint32_t parent = vera_utils_swap_endian32(*property->ptr);
                k_free(property);
                bool is_s_mode = false;
                Base_Node *temp_node = root_node;
                while (1)
                {
                    temp_node = dtb_get_next_wanted_node("interrupt-controller", temp_node, true);
                    if (temp_node == NULL)
                    {
                        break;
                    }
                    char *temp2_compatible = (char *)temp_node->compatible.strings;
                    uint32_t temp2_lenght = temp_node->compatible.elements;
                    while (temp2_lenght)
                    {
                        if (vera_utils_str_cmp(temp2_compatible, IMSICS_Risc_V))
                        {
                            property = dtb_get_node_information(temp_node->node_title, temp_node, "interrupts-extended", string_node);
                            if (property == NULL)
                            {
#if INCLUDE_DEBUG
                                vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                                k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                            }
                            // Check if IMSICs is owned by S Mode
                            for (uint32_t i = 0; i < property->lenght_in_bytes / 4; ++i)
                            {
                                uint32_t temp_reg = vera_utils_swap_endian32(*property->ptr);
                                ++property->ptr;
                                if (temp_reg == 0xb)
                                {
                                    k_free(property);
                                    goto end_of_search;
                                }
                                else if (temp_reg == 0x9)
                                {
                                    k_free(property);
                                    property = dtb_get_node_information(temp_node->node_title, temp_node, "phandle", string_node);
                                    if (property == NULL)
                                    {
#if INCLUDE_DEBUG
                                        vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                                        k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                                    }
                                    if (vera_utils_swap_endian32(*property->ptr) == parent) {
                                        is_s_mode = true;
                                    }
                                    k_free(property);
                                    goto end_of_search;
                                }
                            }
                        }
                        uint32_t str_len = (uint32_t)vera_utils_str_len(temp2_compatible) + 1;
                        temp2_compatible += str_len;
                        temp2_lenght -= str_len;
                    }
                }
                end_of_search:
                if (!is_s_mode)
                {
                    goto Next_Property;
                }


                // Is our own APLIC

                struct APLIC_base_information *base = (struct APLIC_base_information *)k_malloc(sizeof(struct APLIC_base_information));
                if (base == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }
            
                property = dtb_get_node_information(cur_node->node_title, cur_node, "reg", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                base->reg = property->ptr;
                base->byte_lenght = property->lenght_in_bytes;

                k_free(property);
                property = dtb_get_node_information(cur_node->node_title, cur_node, "phandle", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                base->phandle = vera_utils_swap_endian32(*property->ptr);

                k_free(property);
                property = dtb_get_node_information(cur_node->node_title, cur_node, "msi-parent", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                base->msi_parent = vera_utils_swap_endian32(*property->ptr);

                k_free(property);
                property = dtb_get_node_information(cur_node->node_title, cur_node, "riscv,num-sources", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                base->num_sources = vera_utils_swap_endian32(*property->ptr);
                k_free(property);

                base->address_cell = cur_node->address_cell;
                base->size_cell = cur_node->size_cell;
                base->interrupt_cells = cur_node->interrupt_cell;

                init_aplic_informations(base);

            }
            else if (vera_utils_str_cmp(temp_compatible, IMSICS_Risc_V))
            {
                Node_Property *property = dtb_get_node_information(cur_node->node_title, cur_node, "interrupts-extended", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }
                // Check if IMSICs is owned by S Mode
                for (uint32_t i = 0; i < property->lenght_in_bytes / 4; ++i)
                {
                    uint32_t temp_reg = vera_utils_swap_endian32(*property->ptr);
                    ++property->ptr;
                    if (temp_reg == 0xb)
                    {
                        k_free(property);
                        goto Next_Property;
                    }
                    else if (temp_reg == 0x9)
                    {
                        break;
                    }
                }
                // It is owned by S Mode
                k_free(property);

                property = dtb_get_node_information(cur_node->node_title, cur_node, "interrupts-extended", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                struct IMSICS_base_information *base = (struct IMSICS_base_information *)k_malloc(sizeof(struct IMSICS_base_information));
                if (base == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }
                base->interrupted_extended = property->ptr;
                base->lenght_extended = (uint32_t)property->lenght_in_bytes;

                k_free(property);
                property = dtb_get_node_information(cur_node->node_title, cur_node, "reg", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }
                base->address_cell = cur_node->address_cell;
                base->size_cell = cur_node->size_cell;
                base->interrupt_cells = cur_node->interrupt_cell;

                base->reg = property->ptr;
                base->byte_lenght = property->lenght_in_bytes;

                k_free(property);
                property = dtb_get_node_information(cur_node->node_title, cur_node, "riscv,num-ids", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }
                base->id_size = (uint16_t)vera_utils_swap_endian32(*property->ptr);

                k_free(property);
                property = dtb_get_node_information(cur_node->node_title, cur_node, "phandle", string_node);
                if (property == NULL)
                {
#if INCLUDE_DEBUG
                    vera_err_boot_panic(VERA_ERR_NULL_PTR);
#else
                    k_panic_no_write(VERA_ERR_NULL_PTR);
#endif
                }

                base->phandle = vera_utils_swap_endian32(*property->ptr);
                k_free(property);

                IMSIC_init_informations(base);
            }
            else if (vera_utils_str_cmp(temp_compatible, ""))
            {
            }
        Next_Property:
            // Next String from String List
            uint32_t str_len = (uint32_t)vera_utils_str_len(temp_compatible) + 1;
            temp_compatible += str_len;
            temp_lenght -= str_len;
        }
        cur_node = cur_node->next;
    }
#if INCLUDE_DEBUG
    vera_uart_print("Leave dtb_check_drivers\n");
#endif
}

/*
Gets a Property from the Given Node we want to have.

params:
- (ptr) node_title -> UNUSED!!! TODO
- (ptr) info_node -> The node where our Search for the Property Starts
- (ptr) propery_wanted -> The Property we want from the Node
- (ptr) string_node -> To get the Strings

return:
- (ptr) Node_Property -> The Node Property, Pointer to the location and it lenghts
- (ptr) NULL -> Property not Found for the Wished Node

panic:
- VERA_ERR_NULL_PTR -> Param pointer are NULL
- VERA_ERR_NOMEM -> No Memory for allocation
- VERA_ERR_INVAL -> Error Happened, undefinde Error
*/
static Node_Property *dtb_get_node_information(char *node_title, Base_Node *info_node, char *propery_wanted, uint8_t *string_node)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter dtb_get_node_information\n");
#endif
    // Check if everything is fine with the Args
    if (node_title == NULL || info_node == NULL || propery_wanted == NULL || string_node == NULL)
    {
        vera_err_boot_panic(VERA_ERR_NULL_PTR);
    }
    // Our temporary Property
    Node_Property *temp_prop = k_malloc(sizeof(Node_Property));
    if (temp_prop == NULL)
    {
        vera_err_boot_panic(VERA_ERR_NOMEM);
    }
    // Loop Variables for looking up our desired Node
    uint32_t *temp_node = info_node->raw_dtb_node;
    uint32_t depth = 0;
    while (1)
    {
        device_tree_token token = vera_utils_swap_endian32(*temp_node);
        ++temp_node;
        if (token == fdt_begin_node)
        {
            ++depth;
        }
        else if (token == fdt_prop)
        {
            uint32_t length_of_property = vera_utils_swap_endian32(*temp_node);
            ++temp_node;
            char *name_off = ((char *)string_node) + vera_utils_swap_endian32(*temp_node);
            ++temp_node;

            if (depth == 1 && vera_utils_str_cmp(name_off, propery_wanted))
            {
                temp_prop->ptr = temp_node;
                temp_prop->lenght_in_bytes = length_of_property;
                return temp_prop;
            }

            // Nächster Valid token
            uint8_t *temp_ptr = (uint8_t *)temp_node;
            temp_ptr += length_of_property;
            temp_node = (uint32_t *)temp_ptr;
            temp_node = (uint32_t *)vera_utils_align((uint64_t)temp_node, Byte_Size_Per_Token);
        }
        else if (token == fdt_nop || token == fdt_nothing)
        {
            continue;
        }
        else if (token == fdt_end_node)
        {
            --depth;
            if (depth == 0)
            {
                k_free(temp_prop);
                return NULL;
            }
        }
        else if (token == fdt_end)
        {
            k_free(temp_prop);
            vera_err_boot_panic(VERA_ERR_INVAL);
        }
    }
    k_free(temp_prop);
    vera_err_boot_panic(VERA_ERR_INVAL);
}

static Base_Node *dtb_get_next_wanted_node(char *node_title, Base_Node *current_node, bool contains_title)
{
    current_node = current_node->next;
    while (current_node != NULL)
    {
        if (contains_title)
        {
            if (vera_utils_str_has(current_node->node_title, node_title))
            {
                return current_node;
            }
        }
        else
        {
            if (vera_utils_str_cmp(current_node->node_title, node_title))
            {
                return current_node;
            }
        }
        current_node = current_node->next;
    }
    return NULL;
}
