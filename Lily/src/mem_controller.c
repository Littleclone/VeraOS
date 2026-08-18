/* src/memory/mem-controller.c */
#include "../header/mem_controller.h"
#include "../header/lily_extension.h"
#include "../header/paging.h"
#include "../../Kernel/header/kernel.h"

static void mem_translate_nodes(dtb_Memory_Node *nodes, dtb_reserved_entry *reserved_entry, uint16_t *memory_node_counter, mem_node_lily *memory_nodes);

static void mem_prepare_nodes(dtb_tree_lily *tree, mem_node_lily *memory_nodes, uint16_t *memory_node_counter, kernel_area *kernel_region);

static void mem_split_nodes(mem_node_lily *node, mem_node_lily *node_list, uint16_t *counter, phys_address p_adress, uint64_t size, mem_mem_map_lily mem_state);

static void mem_flag_reserve_nodes(mem_node_lily *node, uint16_t *counter);

static void mem_sort_nodes(mem_node_lily *node_list, uint16_t *counter);

#if INCLUDE_DEBUG && INCLUDE_QEMU
static void mem_print(mem_node_lily *node, uint16_t counter);
#endif

/*
    Is Responsible for converting dtb Nodes into Memory nodes and getting the Kernel space AND temp Heap space for later.

    param:
    - info_block -> the information block for the memory nodes.
    - (ptr) kernel_region -> Here the Kernel data is saved, so Lily uses as less space as possible.

    return:
    - (ptr) kernel_boot_info -> A Memory Node Region where the kernel's information are gathered
    - NULL -> Error happened
*/
kernel_boot_info *mem_convert_memory_nodes(mem_convert_infos info_block, kernel_area *kernel_region)
{
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter mem_convert_memory_nodes\n");
#endif

    if (kernel_region == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
// Block to make sure in Print that everything is alright.
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("\nSecure\n\n");
    uint16_t i = 0;
    while (i <= info_block.mem_node_counter)
    {
        lily_uart_printf("Address: %i, Size: %i, State: %i\nReg<", info_block.mem_nodes[i].address, info_block.mem_nodes[i].size, info_block.mem_nodes[i].mem_map_state);

        for (uint32_t j = 0; j < (info_block.mem_nodes[i].length_of_property / 4); ++j)
        {
            lily_uart_printf("%ixX ", lily_utils_swap_endian32(info_block.mem_nodes[i].reg[j]));
        }
        lily_uart_print(">\n");
        ++i;
    }
    i = 0;
    lily_uart_print("\nReserved\n\n");
    do
    {
        lily_uart_printf("Address: %i, Size: %i, State: %i\n", info_block.reserved_nodes[i].address, info_block.reserved_nodes[i].size, dtb_reserved_unknown);
        ++i;
    } while (info_block.reserved_nodes->address != 0 && info_block.reserved_nodes->size != 0);
    lily_uart_print("Finished\n\n");
#endif
    // Real code.
    mem_node_lily memory_nodes[Max_Memory_Nodes];

    // Translate first
    mem_translate_nodes(info_block.mem_nodes, info_block.reserved_nodes, &info_block.mem_node_counter, memory_nodes);

    mem_flag_reserve_nodes(memory_nodes, &info_block.mem_node_counter);

    // Get Kernel node, Temp Heap and a space for Lily to save her Data for the Kernel.
    mem_prepare_nodes(info_block.tree, memory_nodes, &info_block.mem_node_counter, kernel_region);

    // Sort and Merge Memory Nodes.
    mem_sort_nodes(memory_nodes, &info_block.mem_node_counter);

    // Look how much Memory we have in general and reserved, also if we have Min Memory
    mem_node_lily *boot_info_node = NULL;
    for (uint16_t i = 0; i < info_block.mem_node_counter; ++i)
    {
        boot_info_node = &memory_nodes[i];
        if (boot_info_node->mem_state == mem_reserved_kernel_info_lily)
        {
            break;
        }
        boot_info_node = NULL;
    }

    if (boot_info_node == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }

    kernel_boot_info *boot_info = (kernel_boot_info *)boot_info_node->address;

    lily_utils_mem_set((uint8_t *)boot_info, 0, boot_info_node->size);


    // fill the boot informations
    boot_info->kernel_region.bss_end = kernel_region->bss_end;
    boot_info->kernel_region.bss_start = kernel_region->bss_start;
    boot_info->kernel_region.data_end = kernel_region->data_end;
    boot_info->kernel_region.data_start = kernel_region->data_start;
    boot_info->kernel_region.kernel_base_VA = kernel_region->kernel_base_VA;
    boot_info->kernel_region.kernel_end = kernel_region->kernel_end;
    boot_info->kernel_region.kernel_start = kernel_region->kernel_start;
    boot_info->kernel_region.rodata_end = kernel_region->rodata_end;
    boot_info->kernel_region.rodata_start = kernel_region->rodata_start;
    boot_info->kernel_region.sdata_end = kernel_region->sdata_end;
    boot_info->kernel_region.sdata_start = kernel_region->sdata_start;
    boot_info->kernel_region.stack_end = kernel_region->stack_end;
    boot_info->kernel_region.stack_guard_end = kernel_region->stack_guard_end;
    boot_info->kernel_region.stack_guard_start = kernel_region->stack_guard_start;
    boot_info->kernel_region.stack_start = kernel_region->stack_start;
    boot_info->kernel_region.text_end = kernel_region->text_end;
    boot_info->kernel_region.text_start = kernel_region->text_start;

    boot_info->magic[0] = 'L';
    boot_info->magic[1] = 'I';
    boot_info->magic[2] = 'L';
    boot_info->magic[3] = 'Y';
    boot_info->magic[4] = '_';
    boot_info->magic[5] = 'B';
    boot_info->magic[6] = 'O';
    boot_info->magic[7] = 'O';
    boot_info->magic[8] = 'T';
    boot_info->magic[9] = '\0';

    boot_info->mem_nodes_counter = info_block.mem_node_counter;
    mem_node_lily* boot_nodes = (mem_node_lily*)(&boot_info->mem_nodes + sizeof(kernel_boot_info));
    boot_info->mem_nodes = (void*)boot_nodes;

    // Get the Memory Sizes
    for (uint16_t i = 0; i < info_block.mem_node_counter; ++i)
    {
        mem_node_lily *temp_node = &memory_nodes[i];
        boot_info->general_memory += temp_node->size;
        boot_nodes->address = temp_node->address;
        boot_nodes->mem_state = temp_node->mem_state;
        boot_nodes->process_ID = temp_node->process_ID;
        boot_nodes->size = temp_node->size;
        boot_nodes->v_address_internal = temp_node->v_address_internal;
        boot_nodes->v_address_user = temp_node->v_address_user;
        ++boot_nodes;

    }
    if (boot_info->general_memory < To_MiB(512))
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NOMEM);
#else
        lily_panic_no_write(VERA_ERR_NOMEM);
#endif
    }

    mem_node_lily *temp_heap = NULL;
    for (uint16_t i = 0; i < info_block.mem_node_counter; ++i)
    {
        temp_heap = &memory_nodes[i];
        if (temp_heap->mem_state == mem_reserved_kernel_heap_lily)
        {
            break;
        }
        temp_heap = NULL;
    }

    if (temp_heap == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }

    boot_info->temp_heap = (uint8_t*)temp_heap->address;
    boot_info->temp_heap_size = (uint64_t)temp_heap->size;
    boot_info->dtb = (void*)info_block.tree;
    mem_print(memory_nodes, info_block.mem_node_counter);
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave mem_convert_memory_nodes\n");
#endif
    return boot_info;
}

/*
    Translates from dtb-nodes to mem-nodes, also it gives back the number of nodes in the Array, it also counts the memory amount

    param:
    - (ptr) nodes -> The dtb_memory_nodes
    - (ptr) reserved_entry -> the reserved entries in the dtb
    - (ptr) memory_node_counter -> The counter that holds the amount of memory nodes from the dtb.
    - (ptr) memory_nodes -> Saves the Memory Nodes translated inside this array
*/
static void mem_translate_nodes(dtb_Memory_Node *nodes, dtb_reserved_entry *reserved_entry, uint16_t *memory_node_counter, mem_node_lily *memory_nodes)
{
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter mem_tranlate_nodes\n");
#endif
    uint16_t node_counter = 0;
    if (nodes == NULL || reserved_entry == NULL || memory_node_counter == NULL || memory_nodes == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }

    // Insert Reserved Entries inside memory_nodes
    while (reserved_entry->address != 0 || reserved_entry->size != 0)
    {
        memory_nodes[node_counter].address = reserved_entry->address;
        memory_nodes[node_counter].size = reserved_entry->size;
        memory_nodes[node_counter].mem_state = mem_reserved_unknown_lily;
        memory_nodes[node_counter].process_ID = Kernel_Process_ID;
        memory_nodes[node_counter].v_address_internal = 0;
        memory_nodes[node_counter].v_address_user = 0;
        ++reserved_entry;
        ++node_counter;
        if (node_counter >= Max_Memory_Nodes)
        {
#if INCLUDE_DEBUG && INCLUDE_QEMU
            lily_panic(VERA_OVERFLOW);
#else
            lily_panic_no_write(VERA_OVERFLOW);
#endif
        }
    }

    // The Nodes from the DTB get now inserted
    int16_t counter = (int16_t)*memory_node_counter;
    while (counter >= 0)
    {
        // Because we made the DTB Code so its only put Memory nodes inside, we can go in row.
        dtb_Memory_Node *node = nodes;
        ++nodes;

        // Fill the Array with the Informations

        // per Reg Group we need a new Node, For loop!
        for (uint16_t i = 0; i < node->length_of_property / 4; ++i)
        {
            // Get the Address of the Memory Node
            uint64_t adress = (uint64_t)lily_utils_swap_endian32(*node->reg);
            ++node->reg;
            if (node->address == 2)
            {
                adress <<= 32;                                         // Bits nach vorne schieben
                adress |= (uint64_t)lily_utils_swap_endian32(*node->reg); // Untere Bits holen
                ++node->reg;
                ++i;
            }
            // Get the Size Value of the Memory Node
            uint64_t size = (uint64_t)lily_utils_swap_endian32(*node->reg);
            ++node->reg;
            ++i;
            if (node->size == 2)
            {
                size <<= 32;                                         // Bits nach vorne schieben
                size |= (uint64_t)lily_utils_swap_endian32(*node->reg); // Untere Bits holen
                ++node->reg;
                ++i;
            }

            memory_nodes[node_counter].address = adress;
            memory_nodes[node_counter].size = size;

            // Get the Mem State
            if (node->mem_map_state == dtb_free_Memory)
            {
                memory_nodes[node_counter].mem_state = mem_free_memory_dirty_lily;
            }
            else if (node->mem_map_state == dtb_reserved_unknown)
            {
                memory_nodes[node_counter].mem_state = mem_reserved_unknown_lily;
            }
            // init Node Property
            memory_nodes[node_counter].process_ID = Kernel_Process_ID;
            memory_nodes[node_counter].v_address_internal = 0;
            memory_nodes[node_counter].v_address_user = 0;
            ++node_counter;
            if (node_counter >= Max_Memory_Nodes)
            {
#if INCLUDE_DEBUG && INCLUDE_QEMU
                lily_panic(VERA_OVERFLOW);
#else
                lily_panic_no_write(VERA_OVERFLOW);
#endif
            }
        }
        --counter;
    }
    *memory_node_counter = node_counter;
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave mem_tranlate_nodes\n");
#endif
}

/*
    Prepares the Nodes, it splits the Kernel out and then makes Space for the Temp Heap and the Kernel Boot Info Memory Region

    params:
    - (ptr) memory_nodes -> The list with the Memory Nodes the Kernel understands to speak
    - (ptr) memory_node_counter -> The counter that holds the amount of memory nodes from the dtb.
    - (ptr) kernel_region -> The Regions from the Kernel Area in Memory
*/
static void mem_prepare_nodes(dtb_tree_lily *tree, mem_node_lily *memory_nodes, uint16_t *memory_node_counter, kernel_area *kernel_region)
{
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter mem_prepare_nodes\n");
#endif
    if (tree == NULL || memory_nodes == NULL || memory_node_counter == NULL || kernel_region == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }

    uint16_t generic_counter = 0;
    // Find the Kernel and split it out and make it a new Node
    // get the Node where the Kernel is inside.
    mem_node_lily *generic_nodes = NULL;
    for (uint16_t i = 0; i < *memory_node_counter; ++i)
    {
        // the Address Border of the Node
        generic_nodes = &memory_nodes[i];
        uint64_t address_border = generic_nodes->address + generic_nodes->size;
        if (generic_nodes->address <= kernel_region->kernel_start && address_border >= kernel_region->kernel_end)
        {
            break;
        }
        generic_nodes = NULL;
    }

    // Check if we found the target
    if (generic_nodes == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    // Node is available, take it
    mem_split_nodes(generic_nodes, memory_nodes, memory_node_counter, kernel_region->kernel_start, (kernel_region->kernel_end - kernel_region->kernel_start), mem_reserved_kernel_lily);

    // Get Temp Heap
    uint64_t temp_heap_size = 0;
    for (uint16_t i = 0; i < *memory_node_counter; ++i)
    {
        mem_node_lily *node = &memory_nodes[i];
        temp_heap_size += lily_utils_calc_space(node->size, (uint64_t)node->address);
    }
#if INCLUDE_DEBUG && INCLUDE_QEMU
    temp_heap_size += lily_utils_calc_space(0x1000, UART_BASE_QEMU);
#endif
    temp_heap_size += To_MiB(2);

    generic_nodes = NULL;
    for (uint16_t i = 0; i < *memory_node_counter; ++i)
    {
        // the Address Border of the Node
        generic_nodes = &memory_nodes[i];
        if (generic_nodes->mem_state != mem_free_memory_dirty_lily)
        {
            continue;
        }
        if (generic_nodes->size >= temp_heap_size)
        {
            break;
        }
        generic_nodes = NULL;
    }
    // Check if we found the target
    if (generic_nodes == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    // Node is available, take it
    mem_split_nodes(generic_nodes, memory_nodes, memory_node_counter, generic_nodes->address, temp_heap_size, mem_reserved_kernel_heap_lily);

    // Get Kernel Info Block
    uint64_t kernel_info_size = sizeof(mem_node_lily) * Max_Memory_Nodes;
    kernel_info_size += sizeof(kernel_boot_info);

    generic_nodes = NULL;
    for (uint16_t i = 0; i < *memory_node_counter; ++i)
    {
        generic_nodes = &memory_nodes[i];
        if (generic_nodes->mem_state != mem_free_memory_dirty_lily)
        {
            continue;
        }
        if (generic_nodes->size >= kernel_info_size)
        {
            break;
        }
        generic_nodes = NULL;
    }
    // Check if we found the target
    if (generic_nodes == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }

    mem_split_nodes(generic_nodes, memory_nodes, memory_node_counter, generic_nodes->address, kernel_info_size, mem_reserved_kernel_info_lily);

    // Get the Node where the DTB is
    generic_nodes = NULL;
    for (uint16_t i = 0; i < *memory_node_counter; ++i)
    {
        generic_nodes = &memory_nodes[i];
        if ((generic_nodes->address <= (uint64_t)tree && (generic_nodes->address + generic_nodes->size >= (uint64_t)tree)) && generic_nodes->size >= lily_utils_swap_endian32(tree->header.totalsize)) {
            break;
        }
        generic_nodes = NULL;
    }
    // Check if we found the target
    if (generic_nodes == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    mem_split_nodes(generic_nodes, memory_nodes, memory_node_counter, (phys_address)tree, lily_utils_swap_endian32(tree->header.totalsize), mem_reserved_dtb);

#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave mem_prepare_nodes\n");
#endif
}

// --- Helper Function ---

/*
    Splits Nodes to fit

    params:
    - (ptr) node -> The Node where we are inside.
    - (ptr) node_list -> All Nodes
    - (ptr) counter -> The Amount of nodes.
    - p_adress -> The Starting physical adress.
    - size -> The Size of the area we want to claim.
    - mem_state -> The state we want the Memory are to have.
*/
static void mem_split_nodes(mem_node_lily *node, mem_node_lily *node_list, uint16_t *counter, phys_address p_adress, uint64_t size, mem_mem_map_lily mem_state)
{
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter mem_split_nodes\n");
#endif

    if (node == NULL || counter == NULL || p_adress == 0 || size == 0)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    else if ((*counter + 1) >= Max_Memory_Nodes)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_OVERFLOW);
#else
        lily_panic_no_write(VERA_OVERFLOW);
#endif
    }

    size = (uint64_t)lily_utils_align(size, Page_Size);

    uint64_t adress_border = p_adress + size;
    uint64_t node_border = node->address + node->size;

    // if p_address begins on Node Address and end inside or at it
    if (p_adress == node->address && adress_border <= node_border)
    {
        // if the same (unlikely)
        if (adress_border == node_border)
        {
            // Capture the entire Node, EASY, but also unlikely
            node->mem_state = mem_state;
        }
        // Same Start, not same end (More Likely)
        else
        {
            mem_node_lily *adjusted_node = &node_list[*counter];
            ++*counter;
            // Check if Overflow
            if (*counter >= Max_Memory_Nodes)
            {
#if INCLUDE_DEBUG && INCLUDE_QEMU
                lily_panic(VERA_OVERFLOW);
#else
                lily_panic_no_write(VERA_OVERFLOW);
#endif
            }
            // Move the Old Node into the New node
            adjusted_node->address = adress_border;
            adjusted_node->size = node->size - size;
            adjusted_node->mem_state = node->mem_state;
            adjusted_node->process_ID = Kernel_Process_ID;
            adjusted_node->v_address_internal = 0;
            adjusted_node->v_address_user = 0;

            // make the Old node now the new Mem_State and adjust it
            node->address = p_adress;
            node->size = size;
            node->mem_state = mem_state;
            // Finish
        }
    }
    // if p_adress begins inside the node and end inside or at it
    else if (p_adress > node->address && adress_border <= node_border)
    {
        // if the same
        if (adress_border == node_border)
        {
            mem_node_lily *new_node = &node_list[*counter];
            ++*counter;
            // Check if Overflow
            if (*counter >= Max_Memory_Nodes)
            {
#if INCLUDE_DEBUG && INCLUDE_QEMU
                lily_panic(VERA_OVERFLOW);
#else
                lily_panic_no_write(VERA_OVERFLOW);
#endif
            }
            // The new Node gets its properties
            new_node->address = p_adress;
            new_node->size = size;
            new_node->mem_state = mem_state;
            new_node->process_ID = Kernel_Process_ID;
            new_node->v_address_internal = 0;
            new_node->v_address_user = 0;

            // Adjust the old node
            node->size -= size;
            // Finish
        }
        // Not same Start, not same end
        else
        {
            mem_node_lily *new_node = &node_list[*counter];
            ++*counter;
            mem_node_lily *new_end_node = &node_list[*counter];
            ++*counter;
            // Check if Overflow
            if (*counter >= Max_Memory_Nodes)
            {
#if INCLUDE_DEBUG && INCLUDE_QEMU
                lily_panic(VERA_OVERFLOW);
#else
                lily_panic_no_write(VERA_OVERFLOW);
#endif
            }
            // The new Node gets its properties
            new_node->address = p_adress;
            new_node->size = size;
            new_node->mem_state = mem_state;
            new_node->process_ID = Kernel_Process_ID;
            new_node->v_address_internal = 0;
            new_node->v_address_user = 0;

            // calculate difference between the new_node end and the old_node end
            uint64_t difference = node_border - adress_border;

            // Adjust the old node
            node->size -= size + difference;

            // make the new End with properties
            new_end_node->address = adress_border;
            new_end_node->size = difference; // the difference between Address Border and Node Border is the size of the new End Node
            new_end_node->mem_state = node->mem_state;
            new_end_node->process_ID = Kernel_Process_ID;
            new_end_node->v_address_internal = 0;
            new_end_node->v_address_user = 0;
            // Finish
        }
    }
    else
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_INVAL);
#else
        lily_panic_no_write(K_ERR_INVAL);
#endif
    }

#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave mem_split_nodes\n");
#endif
    return;
}

/*
    Takes all Reserved Nodes and looks if they are in a currently as Free Marked Area, if yes, it will get split

    param:
    - (ptr) node -> The List with the Memory Nodes
    - (ptr) counter -> The Counter of the amount of the Memory Node

    - Warning: CPU and Memory Intensive Function, Only use onetime after Translation!
*/
static void mem_flag_reserve_nodes(mem_node_lily *node, uint16_t *counter)
{
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter mem_flag_reserve_nodes\n");
#endif

    if (node == NULL || counter == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    else if (*counter >= Max_Memory_Nodes)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_OVERFLOW);
#else
        lily_panic_no_write(VERA_OVERFLOW);
#endif
    }

    // Loop to check
    for (uint16_t i = 0; i < *counter; ++i)
    {
        mem_node_lily *temp_node = &node[i];

        // If its not Reserved, we don't need it
        if (temp_node->mem_state != mem_reserved_unknown_lily)
        {
            continue;
        }
        uint64_t adress_border = temp_node->address + temp_node->size;
        // Look for Free Nodes
        for (uint16_t j = 0; j < *counter; ++j)
        {
            mem_node_lily *free_node = &node[j];

            if (free_node->mem_state != mem_free_memory_dirty_lily)
            {
                continue;
            }
            uint64_t free_border = free_node->address + free_node->size;
            if (free_node->address <= temp_node->address && adress_border <= free_border)
            {
                // Split
                mem_split_nodes(free_node, node, counter, temp_node->address, temp_node->size, mem_reserved_unknown_lily);
            }
        }
    }
    for (uint16_t i = 0; i < *counter; ++i)
    {
        mem_node_lily *temp_node = &node[i];

        // If its not Reserved, we don't need it
        if (temp_node->mem_state != mem_reserved_unknown_lily)
        {
            continue;
        }
        // Look for Free Nodes
        for (uint16_t j = i + 1; j < *counter; ++j)
        {
            mem_node_lily *free_node = &node[j];
            if (free_node->address == temp_node->address && free_node->size == temp_node->size && free_node->mem_state == temp_node->mem_state)
            {
                free_node->address = 0;
                free_node->size = 0;
                free_node->mem_state = mem_invalid_lily;
            }
        }
    }
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave mem_flag_reserve_nodes\n");
#endif
}

/*
    Sort and merge the Nodes inside the node_list

    params:
    - (ptr) node_list -> the List with the Memory Nodes inside
    - (ptr) counter -> The Counter of the amount of the Memory Node

    - Warning: CPU and Memory Intensive Function, Only use onetime after node prepares.
*/
static void mem_sort_nodes(mem_node_lily *node_list, uint16_t *counter)
{
#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Enter mem_sort_nodes\n");
#endif

    if (node_list == NULL || counter == NULL)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_ERR_NULL_PTR);
#else
        lily_panic_no_write(K_ERR_NULL_PTR);
#endif
    }
    else if (*counter >= Max_Memory_Nodes)
    {
#if INCLUDE_DEBUG && INCLUDE_QEMU
        lily_panic(VERA_OVERFLOW);
#else
        lily_panic_no_write(VERA_OVERFLOW);
#endif
    }

    // Merge all Valid Nodes
    for (uint16_t i = 0; i < *counter; ++i)
    {
        mem_node_lily *temp_node = &node_list[i];
        if (temp_node->mem_state == mem_invalid_lily)
        {
            continue;
        }
        uint64_t adress_border = temp_node->address + temp_node->size;

        for (uint16_t j = 0; j < *counter; ++j)
        {
            mem_node_lily *next_node = &node_list[j];
            if (next_node->mem_state == mem_invalid_lily)
            {
                continue;
            }
            uint64_t next_border = next_node->address + next_node->size;
            // Beginnt am ende des temp Nodes
            if (next_node->address == adress_border && temp_node->mem_state == next_node->mem_state)
            {
                temp_node->size += next_node->size;
                next_node->address = 0;
                next_node->size = 0;
                next_node->mem_state = mem_invalid_lily;
            }
            // Beginnt am ende des next Nodes
            else if (temp_node->address == next_border && temp_node->mem_state == next_node->mem_state)
            {
                temp_node->address = next_node->address;
                temp_node->size += next_node->size;
                next_node->address = 0;
                next_node->size = 0;
                next_node->mem_state = mem_invalid_lily;
            }
        }
    }

    // Sort the Nodes and reconfigure the Counter, Bobble Sort
    uint16_t new_counter = 0;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (uint16_t i = 0; i < *counter; ++i)
        {
            mem_node_lily *temp_node = &node_list[i];
            if (temp_node->mem_state == mem_invalid_lily)
            {
                temp_node->address = UINT64_MAX;
            }
            mem_node_lily *next_node = &node_list[i + 1];
            // Temp ist größer, Wechsel!
            if (temp_node->address > next_node->address)
            {
                // Save Data
                changed = true;
                mem_node_lily temp_holder;
                temp_holder.address = temp_node->address;
                temp_holder.mem_state = temp_node->mem_state;
                temp_holder.size = temp_node->size;

                // Smaller Node infront!
                temp_node->address = next_node->address;
                temp_node->mem_state = next_node->mem_state;
                temp_node->size = next_node->size;

                // Back
                next_node->address = temp_holder.address;
                next_node->mem_state = temp_holder.mem_state;
                next_node->size = temp_holder.size;
            }
        }
    }
    // Security Check every Node with proper Init Settings, virtuell and Process_ID
    for (uint16_t i = 0; i < *counter; ++i)
    {
        mem_node_lily *temp_node = &node_list[i];
        if (temp_node->mem_state == mem_invalid_lily)
        {
            break;
        }
        ++new_counter;
        temp_node->process_ID = Kernel_Process_ID;
        temp_node->v_address_internal = 0;
        temp_node->v_address_user = 0;
    }
    *counter = new_counter;

#if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("Leave mem_sort_nodes\n");
#endif
}

#if INCLUDE_DEBUG && INCLUDE_QEMU
static void mem_print(mem_node_lily *node, uint16_t counter)
{
    if (node == NULL || counter == 0)
    {
        lily_panic(VERA_ERR_NULL_PTR);
    }
    lily_uart_print("\nStart:\n");
    for (uint16_t i = 0; i < counter; ++i)
    {
        lily_uart_printf("Node: %i\n Adress: %p, Size, %p, Border: %p, State: %i\n\n", i, node[i].address, node[i].size, (node[i].address + node[i].size), node[i].mem_state);
    }
    lily_uart_print("Finish\n");
}
#endif
