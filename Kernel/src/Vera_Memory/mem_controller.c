/* src/Vera_Memory/mem_controller.c */
#include "../../header/Vera_Memory/mem_controller.h"
#include "../../header/Vera_Memory/allocator.h"
#include "../../header/Vera_Memory/paging.h"
#include "../../header/Vera_Device_Driver/dtb_parser.h"
#include "../../header/Vera_UART/uart.h"

// Static Variables

static vera_utils_safe_array P_Used_Mem_nodes;
static vera_utils_safe_array P_Free_Mem_nodes;
static mem_node *P_cur_used_mem_map_node = NULL; // Die derzeitige Node die die Used Memory Map beinhaltet
static mem_node *P_cur_free_mem_map_node = NULL; // Die Derzeitige Node die die Free Memory Map beinhaltet

static paging_table_container P_Kernel_table; // Der Kernel Page Table

static memory P_Free_memory;         // The Free Available Memory
static memory P_Used_memory_general; // The Used Memory counted on every system
static memory P_Used_memory_kernel;  // The Used Memory by the System / Kernel
static memory P_Losed_memory;        // The Memory we losed because of Alignement Issues

// Kernel Setup

static vera_state mem_first_paging(kernel_boot_info *const boot_info, paging_table_container*const k_table);

// Helper Functions

inline static void mem_get_memory_map_counter(vera_utils_safe_array *mem_nodes);
static void mem_merge_memory_map(vera_utils_safe_array *holder);
static mem_node *mem_split_mem_area(paging_table_container *const process_table, mem_node *const next_node, const uint64_t pages_needed, const mem_mem_map mem_state, const I_PID i_pid, const bool needs_Split, const bool is_kernel);
static void mem_make_mem_map(vera_utils_safe_array *mem_nodes_holder, mem_node **holder_node);
static mem_node *mem_get_free_node(mem_node *memory_nodes);

#define Mem_Node_not_used 0

/*
Wird beim Kernel Init aufgerufen, diese Funktion Initialisiert den Kernel Root Page Table
und Mapped UART und dtb

params:
- (const) (ptr) boot_info -> der Pointer zu den Kernel boot infos, übergeben von "Lily"
- (const) (ptr) dtb_header_ptr -> der Pointer zum dtb_header, dies ist auch der anfang des dtb

Panic:
- Status zurück gegeben von paging_map_page oder von mem_first_paging

return:
- K_OK -> Alles okay
*/
vera_state mem_init_kernel(kernel_boot_info *const boot_info, void *const dtb_header_ptr)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter mem_init_kernel\n");
#endif
    // Prepare for Paging
    dtb_header *header_ptr = (dtb_header *)dtb_header_ptr;

    P_Kernel_table.root_table = (page_table*)vera_utils_align((uint64_t)(boot_info->temp_heap), Page_Size);
    P_Kernel_table.next_free_table = NULL;


    vera_state status = VERA_OK;
    /* Kernel First Paging Kernel muss einmal gemapped werden mit Temp Heap und danach in einem Richtigem Heap */
    status = mem_first_paging(boot_info, &P_Kernel_table);
    if (VERA_FAILED(status))
    {
        vera_err_boot_panic(status);
    }
// Temp: UART Again paged.
#if INCLUDE_DEBUG
    status = paging_map_pages_Sv39(&P_Kernel_table, (phys_address)UART_BASE_QEMU, (virt_address)UART_BASE_QEMU, 1, (paging_Valid | paging_Read | paging_Write | paging_Global | paging_Accessed | paging_Dirty), true, false);
    if (VERA_FAILED(status))
    {
        vera_err_boot_panic(status);
    }
#endif

    // Save the Nodes in the Right Static Pointer:

    mem_node temp_used_holder[Max_Memory_Nodes];
    mem_node temp_free_holder[Max_Memory_Nodes];

    P_Used_Mem_nodes.ptr_array = (void *)temp_used_holder;
    P_Used_Mem_nodes.max_elements = Max_Memory_Nodes; // Set by Lily as default

    P_Free_Mem_nodes.ptr_array = (void *)temp_free_holder;
    P_Free_Mem_nodes.max_elements = Max_Memory_Nodes; // Set by Lily as default

    // Save the nodes in the Global Safe Array's

    uint8_t counter_free = 0;
    uint8_t counter_used = 0;
    uint16_t counter = 0;
    mem_node *temp_holder = (mem_node *)boot_info->mem_nodes;
    while (temp_holder->mem_state != mem_flag_invalid || counter < boot_info->mem_nodes_counter)
    {
        if (temp_holder->mem_state == mem_flag_free_memory_clean || temp_holder->mem_state == mem_flag_free_memory_dirty)
        {
            // Free nodes getting saved
            temp_free_holder[counter_free].address = temp_holder->address;
            temp_free_holder[counter_free].mem_state = temp_holder->mem_state;
            temp_free_holder[counter_free].process_ID = temp_holder->process_ID;
            temp_free_holder[counter_free].size = temp_holder->size;
            temp_free_holder[counter_free].v_address_internal = temp_holder->v_address_internal;
            temp_free_holder[counter_free].v_address_user = temp_holder->v_address_user;
            ++counter_free;
        }
        else
        {
            // Used Nodes getting saved
            temp_used_holder[counter_used].address = temp_holder->address;
            temp_used_holder[counter_used].mem_state = temp_holder->mem_state;
            temp_used_holder[counter_used].process_ID = temp_holder->process_ID;
            temp_used_holder[counter_used].size = temp_holder->size;
            temp_used_holder[counter_used].v_address_internal = temp_holder->v_address_internal;
            temp_used_holder[counter_used].v_address_user = temp_holder->v_address_user;
            ++counter_used;
        }
        ++counter;
        ++temp_holder;
    }
    // Local Pointer to the Global Pointer
    P_Free_Mem_nodes.counter = counter_free;
    P_Used_Mem_nodes.counter = counter_used;

#if INCLUDE_DEBUG
    vera_uart_print("Memory_Paging Activates soon!\n");
#endif
    activatePaging(P_Kernel_table.root_table, 0);
#if INCLUDE_DEBUG
    vera_uart_print("Paging is Active!\n");
#endif

    // Convert P_Kernel_Table pointer to Virtuellen Pointer

    mem_node *temps = (mem_node *)P_Used_Mem_nodes.ptr_array;
    while (temps->mem_state != mem_flag_reserved_kernel_page_table)
    {
        ++temps;
    }
    P_Kernel_table.next_free_table = (uint8_t *)(((uint64_t)P_Kernel_table.next_free_table - (uint64_t)P_Kernel_table.root_table) + temps->v_address_internal);
    P_Kernel_table.root_table = (page_table *)temps->v_address_internal;

    // Sort Memory Map und put it in a New Node.

    mem_get_memory_map_counter(&P_Used_Mem_nodes);
    mem_get_memory_map_counter(&P_Free_Mem_nodes);
    mem_make_mem_map(&P_Used_Mem_nodes, &P_cur_used_mem_map_node);
    mem_make_mem_map(&P_Free_Mem_nodes, &P_cur_free_mem_map_node);
    mem_merge_memory_map(&P_Used_Mem_nodes);
    mem_merge_memory_map(&P_Free_Mem_nodes);
    return status;
}

// --- Memory Map Kernel functions ---

/*
Funktion für den Allokator, diese gibt anhand der Requested Bytes ein pointer zur Pages die minimal "requested_bytes" groß ist und bis
zur nächsten Page Align aufgerundet ist. Memory Node das Kernel Heap ist, wird als Kernel Heap markiert.

params:
- (const) requested_bytes -> Die Anzahl an Bytes die wir gerne allokieren wollen würden

return:
- mem_node ptr zu den neuen Heap Pages
- NULL wenn keine bytes requested wurden

warning:
- Kann eventuell "mem_make_mem_map" aufrufen, dies ist eine Memory Intensive opterationen.

todo:
- vielleicht in general request_pages machen?
*/
mem_node *k_mem_request_heap_page(const size_t requested_bytes)
{
    if (requested_bytes == 0)
    {
        return NULL;
    }

#if INCLUDE_DEBUG
    vera_uart_print("Enter k_mem_request_heap_page\n");
#endif

    // Pages die wir brauchen ausrechnen
    uint64_t pages_needed = (uint64_t)(requested_bytes / Page_Size);
    if ((requested_bytes % Page_Size) != 0)
    {
        ++pages_needed;
    }
    // Such nach einem Anliegendem Freiem Memory bereich
    mem_node *memory_nodes = (mem_node *)P_Free_Mem_nodes.ptr_array;
    uint64_t elements = P_Free_Mem_nodes.counter;
    uint64_t max_element = P_Free_Mem_nodes.max_elements;
    if (elements >= max_element)
    {
// Neuer Ort fürs Array wird initialisiert.
#if INCLUDE_DEBUG
        vera_uart_print("k_mem_request_heap_page, expand memory map\n");
#endif
        mem_make_mem_map(&P_Free_Mem_nodes, &P_cur_free_mem_map_node);
        memory_nodes = (mem_node *)P_Free_Mem_nodes.ptr_array;
        elements = P_Free_Mem_nodes.counter;
        max_element = P_Free_Mem_nodes.max_elements;
    }

    // First Fit
    for (uint64_t i = 0; i < max_element; ++i)
    {
        mem_node *cur_node = &memory_nodes[i];
        if (cur_node->mem_state != mem_flag_free_memory_clean && cur_node->mem_state != mem_flag_free_memory_dirty)
        {
            continue;
        }
        uint64_t get_pages = cur_node->size / Page_Size;
        if (get_pages > pages_needed)
        {
#if INCLUDE_DEBUG
            vera_uart_print("Leave k_mem_request_heap_page\n");
#endif
            return mem_split_mem_area(NULL, cur_node, pages_needed, mem_flag_reserved_kernel_heap, Kernel_Process_ID, true, true);
        }
        else if (get_pages == pages_needed)
        {
#if INCLUDE_DEBUG
            vera_uart_print("Leave k_mem_request_heap_page\n");
#endif
            return mem_split_mem_area(NULL, cur_node, pages_needed, mem_flag_reserved_kernel_heap, Kernel_Process_ID, false, true);
        }
    }

#if INCLUDE_DEBUG
    vera_uart_print("Leave k_mem_request_heap_Page: NULL\n");
#endif
    return NULL;
}

/*
Pages können hier wieder freigegeben werden, diese werden anhand der v_address und I_PID bestimmt. Diese können danach
wieder als Freier Memory benutzt werden. Diese können mitten in einem Node sein
params:
- (const) v_address -> Startende Virtuelle Addresse der Memory Node
- area_size -> Die größe in Bytes der Memory Node
- (const) from_intern_process_ID -> Die Interne Process ID die diesem memory bereich besitzt
- (const) process_paging_table_container -> Der Table vom Process wo Pages free gemacht werden sollen
- (const) kernel_pages -> ob diese Pages dem kernel gehören

return:
- K_OK -> Alles okay
- VERA_ERR_NULL_PTR -> Wenn !kernel_Pages:_ process_paging_table_container ist NULL oder from_intern_process_ID ist 0
- VERA_ERR_INVAL -> Target Node wurde nicht gefunden oder v_address/area_size ist 0
- VERA_OVERFLOW -> v_address und area size combined wird zum Integer Overflow
- State -> Je nachdem was "paging_edit_pages" ausgibt

warning:
- Kann eventuell "mem_make_mem_map" aufrufen, dies ist eine Memory Intensive opterationen.
*/
vera_state k_mem_free_pages(const virt_address v_addresse, uint64_t area_size, const I_PID from_intern_process_ID, void *const process_paging_table_container, const bool kernel_pages)
{
    if (!kernel_pages && (process_paging_table_container == NULL || from_intern_process_ID == 0))
    {
        return VERA_ERR_NULL_PTR;
    }
    else if (v_addresse == 0 || area_size == 0)
    {
        return VERA_ERR_INVAL;
    }
    uint64_t overflow_test = v_addresse + area_size;

    if (overflow_test < v_addresse)
    {
        return VERA_OVERFLOW;
    }

#if INCLUDE_DEBUG
    vera_uart_print("Enter k_mem_free_pages\n");
#endif
    uint64_t elements = P_Used_Mem_nodes.counter;
    uint64_t max_elements = P_Used_Mem_nodes.max_elements;
    // Falls wir schon voll sind, mach eine vergrößerte
    if (elements >= max_elements)
    {
        mem_make_mem_map(&P_Used_Mem_nodes, &P_cur_used_mem_map_node);
        elements = P_Used_Mem_nodes.counter;
        max_elements = P_Used_Mem_nodes.max_elements;
    }
    mem_node *nodes = (mem_node *)P_Used_Mem_nodes.ptr_array;
    mem_node *target_node = NULL;

    area_size = vera_utils_align(area_size, Page_Size);

    // Wir holen uns die Address Broder der Node die wir suchen

    uint64_t address_border = v_addresse + area_size;

    // Finde die Node mit den der die Pages beinhaltet die wir freigeben wollen.
    mem_node *holder = nodes;
    uint64_t counter = 0;
    while (elements && counter != max_elements)
    {
        ++counter;
        if (holder->size == Mem_Node_not_used)
        {
            ++holder;
            continue;
        }
        --elements;
        uint64_t node_v_address = 0;
        uint64_t node_address_border = 0;
        if (kernel_pages)
        {
            node_address_border = holder->v_address_internal + holder->size;
            node_v_address = holder->v_address_internal;
        }
        else
        {
            node_address_border = holder->v_address_user + holder->size;
            node_v_address = holder->v_address_user;
        }
        if (holder->process_ID == from_intern_process_ID && ((node_v_address <= v_addresse) && (node_address_border >= address_border)) && (holder->mem_state != mem_flag_free_memory_clean || holder->mem_state != mem_flag_free_memory_dirty))
        {
            target_node = holder;
            break;
        }
        ++holder;
    }

    if (target_node == NULL)
    {
#if INCLUDE_DEBUG
        vera_uart_print("Free_pages:_ No target_node found (NULL)\n");
#endif
        return VERA_ERR_INVAL;
    }

    // Target Node ist nun entweder mitten in einer Node oder am anfang der Node

    uint64_t node_v_address = 0;
    uint64_t node_address_border = 0;

    if (kernel_pages)
    {
        node_address_border = target_node->v_address_internal + target_node->size;
        node_v_address = target_node->v_address_internal;
    }
    else
    {
        node_address_border = target_node->v_address_user + target_node->size;
        node_v_address = target_node->v_address_user;
    }

    // Hält den freien Platz in der Free list
    mem_node *temp_holder = mem_get_free_node((mem_node *)P_Free_Mem_nodes.ptr_array);
    ++P_Free_Mem_nodes.counter;
#if INCLUDE_DEBUG
    vera_uart_print("Enter free_pages:_\n");
#endif
    // Node ist am Anfang
    if (node_v_address == v_addresse)
    {
        // Node Endet mitten Drin
        if (node_address_border > address_border)
        {
#if INCLUDE_DEBUG
            vera_uart_print("Node endet mittendrin\n");
#endif
            // Speichere dinge in Temp
            temp_holder->address = target_node->address;
            temp_holder->v_address_internal = target_node->v_address_internal;
            temp_holder->size = area_size;
            temp_holder->mem_state = mem_flag_free_memory_dirty;

            // Aktualisiere Target (Von dem Memory Area free wird)
            target_node->address += area_size;
            target_node->size -= area_size;
            target_node->v_address_internal += area_size;
            if (!kernel_pages)
            {
                target_node->v_address_user += area_size;
            }
        }
        // Ist gleich Groß
        else if (node_address_border == address_border)
        {
#if INCLUDE_DEBUG
            vera_uart_print("Gleich Groß\n");
#endif
            temp_holder->address = target_node->address;
            temp_holder->mem_state = mem_flag_free_memory_dirty;
            temp_holder->process_ID = Kernel_Process_ID;
            temp_holder->size = target_node->size;
            temp_holder->v_address_internal = target_node->v_address_internal;

            // Zeroing out:
            target_node->address = Mem_Node_not_used;
            target_node->mem_state = mem_flag_invalid;
            target_node->process_ID = Mem_Node_not_used;
            target_node->size = Mem_Node_not_used;
            target_node->v_address_internal = Mem_Node_not_used;
            --P_Used_Mem_nodes.counter;
        }
    }
    // Beginnt mitten drin, endet mitten drin / am ende
    else if (node_v_address < v_addresse && node_address_border >= address_border)
    {
        // Beginnt drinne, endet am ende
        if (node_address_border == address_border)
        {
#if INCLUDE_DEBUG
            vera_uart_print("Beginnt Mittendrin, Endet am ende\n");
#endif
            // Speicher in Free
            temp_holder->address = target_node->address + (target_node->size - area_size);
            temp_holder->v_address_internal = target_node->v_address_internal + (target_node->size - area_size);
            temp_holder->mem_state = mem_flag_free_memory_dirty;
            temp_holder->process_ID = Kernel_Process_ID;
            temp_holder->size = area_size;

            // Split Target Node
            target_node->size -= area_size;
        }
        // Endet mitten drin
        else
        {
#if INCLUDE_DEBUG
            vera_uart_print("Endet mitten drin\n");
#endif
            mem_node *temp_holder_end = mem_get_free_node((mem_node *)P_Used_Mem_nodes.ptr_array);
            ++P_Used_Mem_nodes.counter;

            // Free Mem Node machen
            temp_holder->address = target_node->address + (target_node->size - area_size);
            temp_holder->v_address_internal = target_node->v_address_internal + (target_node->size - area_size);
            temp_holder->mem_state = mem_flag_free_memory_dirty;
            temp_holder->process_ID = Kernel_Process_ID;
            temp_holder->size = area_size;

            // Split Target Node
            target_node->size = (temp_holder->address - target_node->address);

            // Create End Node, because Free took in the middle
            temp_holder_end->address = temp_holder->address + area_size;
            temp_holder_end->process_ID = target_node->process_ID;
            temp_holder_end->mem_state = target_node->mem_state;
            temp_holder_end->v_address_internal = temp_holder->v_address_internal + area_size;
            if (!kernel_pages)
            {
                temp_holder_end->v_address_user = target_node->v_address_user + (target_node->size + area_size);
            }
        }
    }
    else
    {
        return VERA_ERR_INVAL;
    }
#if INCLUDE_DEBUG
    vera_uart_print("Page wird free'd\n");
#endif
    // Free Page wird nun auch im Paging Free gemacht
    uint64_t needing_pages = area_size / Page_Size;
    vera_state state = VERA_OK;
    for (uint64_t i = 0; i < needing_pages; ++i)
    {
        if (!kernel_pages)
        {
            state = paging_edit_page_Sv39((paging_table_container *)process_paging_table_container, temp_holder->v_address_internal + (i * Page_Size), 0, Kernel_memory_map_flags, true);
            if (VERA_FAILED(state))
            {
                return state;
            }
        }
        else
        {
            state = paging_edit_page_Sv39(&P_Kernel_table, temp_holder->v_address_internal + (i * Page_Size), 0, Kernel_memory_map_flags, true);
            if (VERA_FAILED(state))
            {
                return state;
            }
        }
    }

    // --- Mergen ---
    address_border = temp_holder->address + temp_holder->size;

    // Und sicherheit für kein Out of Bounds read/write
    uint64_t itteration_counter = 0;
    elements = P_Free_Mem_nodes.counter;

    while (elements && itteration_counter >= max_elements)
    {
        mem_node *node = &nodes[itteration_counter];
        ++itteration_counter;
        if (node->size == Mem_Node_not_used)
        {
            continue;
        }
        --elements;
        uint64_t node_address_border = node->address + node->size;

        // Passt es zu unserer Node?
        if (temp_holder->address == node_address_border)
        {
            // Falls unser node wo startet wo ein andere freier endet.
            temp_holder->address = node->address;
            temp_holder->size += node->size;
            node->size = Mem_Node_not_used;
            node->address = Mem_Node_not_used;
            --P_Free_Mem_nodes.counter;
        }
        else if (address_border == node->address)
        {
            // Falls unser bereich wo endet wo ein anderer freier startet.
            temp_holder->size += node->size;
            node->size = Mem_Node_not_used;
            node->address = Mem_Node_not_used;
            --P_Free_Mem_nodes.counter;
        }
    }

#if INCLUDE_DEBUG
    vera_uart_print("Leave k_mem_free_pages\n");
#endif
    return VERA_OK;
}

/*
    Pages the Memory Addresses from MMIO into the Kernel Page Table

    params:
    - (const) p_address -> The Starting Physical Address of the region.
    - (const) v_address -> The Starting Virtuell Address of the region.
    - (const) area_size -> The Size of the Area

    return:
    - K_OK -> Everything fine.
    - state -> vera_state from "paging_map_pages_Sv39"
*/
vera_state k_mem_page_new_driver_area(const phys_address p_address, const virt_address v_address, const uint64_t area_size)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter k_mem_page_new_driver_area\n");
#endif
    // Map the Pages for the Driver to be accessible
    uint32_t pages_need = area_size / Page_Size;
    if ((p_address % Page_Size) != 0)
    {
        ++pages_need;
    }
    vera_state state = paging_map_pages_Sv39(&P_Kernel_table, p_address, v_address, pages_need, Kernel_data_flags, true, true);
    flushTLB_Cache();
    if (VERA_FAILED(state))
    {
        return state;
    }

#if INCLUDE_DEBUG
    vera_uart_print("Leave k_mem_page_new_driver_area\n");
#endif
    return VERA_OK;
}

/*
    Unpages the Memory Addresses from MMIO from the Kernel Page Table

    params:
    - (const) p_address -> The Starting Physical Address of the region.
    - (const) v_address -> The Starting Virtuell Address of the region.
    - (const) area_size -> The Size of the Area

    return:
    - K_OK -> Everything fine.
    - state -> vera_state from "paging_map_pages_Sv39"
*/
vera_state k_mem_unpage_new_driver_area(const phys_address p_address, const virt_address v_address, const uint64_t area_size)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter k_mem_unpage_new_driver_area\n");
#endif
    // Map the Pages for the Driver to be accessible
    uint32_t pages_need = area_size / Page_Size;
    if ((p_address % Page_Size) != 0)
    {
        ++pages_need;
    }
    for (uint32_t i = 0; i < pages_need; ++i)
    {
        vera_state state = paging_edit_page_Sv39(&P_Kernel_table, v_address + (i * Page_Size), 0, Kernel_memory_map_flags, true);
        if (VERA_FAILED(state))
        {
            return state;
        }
    }
    flushTLB_Cache();

#if INCLUDE_DEBUG
    vera_uart_print("Leave k_mem_unpage_new_driver_area\n");
#endif
    return VERA_OK;
}

// ------

// Helper Function

/*
Zählt alle derzeit genutzen Memory Nodes bis es auf den Ersten "INVALID" memory node trifft,
diese zahl wird im Safe Array mem_nodes (param) gespeichert.

params:
- (ptr) Pointer to the P_Mem_Node List, we want to count
*/
inline static void mem_get_memory_map_counter(vera_utils_safe_array *mem_nodes)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter mem_get_memory_map_counter\n");
#endif
    uint64_t counter = 0;
    mem_node *cur_node = (mem_node *)mem_nodes->ptr_array;
    mem_node *end = (mem_node *)(((mem_node *)mem_nodes->ptr_array) + mem_nodes->max_elements);
    while (cur_node != end)
    {
        if (cur_node->size == 0)
        {
            ++counter;
        }
        ++cur_node;
    }
    mem_nodes->counter = counter;
#if INCLUDE_DEBUG
    vera_uart_print("Leave mem_get_memory_map_counter\n");
#endif
}

/*
Merged Memory bereiche zusammen die aneinander grenzen und zueinander passen.

warning:
- CPU Intensiv!

pre:
- Funktioniert am Besten wenn davor "mem_sort_memory_map" ausgeführt wurde

post:
- Wird automatisch "mem_sort_memory_map" aufrufen und danach "mem_get_memory_map_counter"
*/
static void mem_merge_memory_map(vera_utils_safe_array *holder)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter mem_merge_memory_map [NOT MUCH USED CURRENTLY!]\n");
#endif
    mem_get_memory_map_counter(holder);
    uint64_t mem_node_counter = holder->max_elements;
    mem_node *memory_node = (mem_node *)holder->ptr_array;

    for (uint64_t i = 0; i < mem_node_counter; ++i)
    {
        mem_node *cur_node = &memory_node[i];
        for (uint64_t j = 0; j < mem_node_counter; ++j)
        {
            mem_node *next_node = &memory_node[j];
            uint64_t cur_address_border = cur_node->address + cur_node->size;
            if (cur_address_border == next_node->address && cur_node->mem_state == next_node->mem_state && cur_node->process_ID == next_node->process_ID)
            {
                cur_node->size += next_node->size;
                next_node->address = UINT64_MAX;
                next_node->v_address_internal = 0;
                next_node->v_address_user = 0;
                next_node->mem_state = mem_flag_invalid;
            }
        }
    }
    mem_get_memory_map_counter(holder);
#if INCLUDE_DEBUG
    vera_uart_print("Leave mem_merge_memory_map\n");
#endif
}

/*
Schneidet ein Node aus einem Memory Bereich raus und splitted die in 2 neue Memory Nodes, 1 wird auf die
memory Map Liste addiert.

params:
- (const) (ptr) next_node -> Der Memory Node der gesplittet werden soll
- (const) pages_needed -> Die anzahl an Pages (und daraus dann auch bytes) die der neue Node braucht
- (const) mem_state -> den State den der neue Memory Node haben soll
- (const) i_pid -> den Interne Process ID die der neue bereich haben soll
- (const) needs_Split -> Entscheided ob überhaupt gesplittet werden soll oder übernimmen wird
- (const) is_kernel -> Entscheidt ob v_address internal oder user zurück gegeben wird

returns:
- Gibt die Mem Node zurück das allokiert wurde
- NULL -> Wenn kein Process Table mitgegeben wird, obwohl es nicht zum Kernel gehört
*/
static mem_node *mem_split_mem_area(paging_table_container *const process_table, mem_node *const next_node, const uint64_t pages_needed, const mem_mem_map mem_state, const I_PID i_pid, const bool needs_Split, const bool is_kernel)
{
    // Basic Check for now.
    if (next_node == NULL || pages_needed == 0 || mem_state == mem_flag_invalid || (!is_kernel && process_table == NULL))
    {
        return NULL;
    }

#if INCLUDE_DEBUG
    vera_uart_print("Enter mem_split_mem_area\n");
#endif

    // Init
    mem_node *memory_nodes = (mem_node *)P_Used_Mem_nodes.ptr_array;
    uint64_t elements = P_Used_Mem_nodes.counter;
    uint64_t max_element = P_Used_Mem_nodes.max_elements;
    // Wir müssen den bereich rauschneiden und als Memory Node addieren
    if (needs_Split)
    {
        // Put in new Node the informations
        memory_nodes = mem_get_free_node(memory_nodes);
        memory_nodes->address = next_node->address;
        ;
        memory_nodes->size = pages_needed * Page_Size;
        memory_nodes->v_address_internal = next_node->v_address_internal;
        memory_nodes->v_address_user = next_node->v_address_user;
        memory_nodes->mem_state = mem_state;
        memory_nodes->process_ID = i_pid;

        // Split
        next_node->address += memory_nodes->size;
        next_node->v_address_internal += memory_nodes->size;
        next_node->size -= memory_nodes->size;

        ++P_Used_Mem_nodes.counter;

        if (is_kernel)
        {
            for (uint64_t i = 0; i < pages_needed; ++i)
            {
                paging_edit_page_Sv39(&P_Kernel_table, memory_nodes->v_address_internal + (i * Page_Size), 0, Kernel_data_flags, true);
            }
            // Dirty?
            if (next_node->mem_state == mem_flag_free_memory_dirty)
            {
                vera_utils_mem_set_64((uint64_t *)memory_nodes->v_address_internal, 0, memory_nodes->size);
            }
#if INCLUDE_DEBUG
            vera_uart_print("Leave mem_split_mem_area\n");
#endif
            return memory_nodes;
        }
        else
        {
            vera_err_boot_panic(VERA_ERR_NOSUP);
            vera_state state = VERA_OK;
            for (uint64_t i = 0; i < pages_needed; ++i)
            {
                state = paging_edit_page_Sv39(process_table, memory_nodes->v_address_internal + (i * Page_Size), 0, 0, true);
                if (state == VERA_ERR_INVAL)
                {
                    // paging_map_pages_Sv39();
                }
            }
            return &memory_nodes[elements];
        }
    }

    // Wenn Bereich Gleich Groß wie benötigter Space ist, dann übernimm gesammten Node
    else
    {
        // The new Node in P_Used where the Node is stored.
        memory_nodes = mem_get_free_node(memory_nodes);
        memory_nodes->address = next_node->address;
        memory_nodes->mem_state = mem_state;
        memory_nodes->process_ID = i_pid;
        memory_nodes->size = next_node->size;
        memory_nodes->v_address_internal = next_node->v_address_internal;
        memory_nodes->v_address_user = next_node->v_address_user;

        // Next Node Nullen to invalided it
        next_node->address = 0;
        next_node->v_address_internal = 0;
        next_node->mem_state = mem_flag_invalid;

        --P_Free_Mem_nodes.counter;
        ++P_Used_Mem_nodes.counter;
        if (is_kernel)
        {
            for (uint64_t i = 0; i < pages_needed; ++i)
            {
                paging_edit_page_Sv39(&P_Kernel_table, memory_nodes->v_address_internal + (i * Page_Size), 0, Kernel_data_flags, true);
            }
            // Memory Area have to be clean
            if (memory_nodes->mem_state == mem_flag_free_memory_dirty)
            {
                vera_utils_mem_set_64((uint64_t *)memory_nodes->address, 0, memory_nodes->size);
            }
#if INCLUDE_DEBUG
            vera_uart_print("Leave mem_split_mem_area\n");
#endif
            return memory_nodes;
        }
        else
        {
            vera_err_boot_panic(VERA_ERR_NOSUP);
            for (uint64_t i = 0; i < pages_needed; ++i)
            {
                paging_edit_page_Sv39(process_table, memory_nodes->v_address_internal + (i * Page_Size), 0, 0, true);
            }
            return memory_nodes;
        }
    }
}

/*
Unused currently
*/
static mem_node *mem_cut_mem_area(mem_node *node_to_cut, phys_address p_address, virt_address v_address, uint64_t area_size)
{
    return NULL;
}
/*
Diese funktion wird aufgerufen wenn die Memory Map erweitert werden muss, diese sollte mindestens 1 mehr Page sein als das
derzeitige Limit

params:
- (ptr) mem_nodes_holder -> The Holder of the current array holder
- (ptr)(ptr) holder_node -> The Holder of the current Map

panic:
- VERA_ERR_NOMEM -> Wenn wir keine erweiterte Memory Map erstellen konnten

warning:
- Memory intensive Funktion!

*/
static void mem_make_mem_map(vera_utils_safe_array *mem_nodes_holder, mem_node **holder_node)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter mem_make_mem_map\n");
#endif

    if (mem_nodes_holder == NULL || mem_nodes_holder->ptr_array == NULL || holder_node == NULL)
    {
        vera_err_boot_panic(VERA_ERR_NOMEM);
    }

    uint64_t element = (uint64_t)mem_nodes_holder->counter;
    uint64_t temp = (element * sizeof(mem_node));
    uint64_t pages_min = temp / Page_Size;
    if (temp % Page_Size)
    {
        // Bring es auf eine Saubere Page Grenze (Sollte bei mem_map immer der fall sein, ist vorsichtshalber dabei)
        ++pages_min;
    }
    pages_min += 1;

    // Search for a Free node we can use
    mem_node *free_mem_nodes = (mem_node *)P_Free_Mem_nodes.ptr_array;
    while (element)
    {
        if (free_mem_nodes->mem_state != mem_flag_free_memory_clean && free_mem_nodes->mem_state != mem_flag_free_memory_dirty)
        {
            if (free_mem_nodes->size != 0)
            {
                --element;
            }
            ++free_mem_nodes;
            continue;
        }
        uint64_t space_in_pages = free_mem_nodes->size / Page_Size;
        if (space_in_pages > pages_min)
        {
#if INCLUDE_DEBUG
            vera_uart_printf("new_mem_map, split:_ Pages_min: %il\n", pages_min);
#endif
            // init new Node with the Memory Map
            mem_node cur_node;
            cur_node.address = free_mem_nodes->address;
            cur_node.size = pages_min * Page_Size;
            cur_node.process_ID = Kernel_Process_ID;
            cur_node.mem_state = mem_flag_reserved_memory_system_map;
            cur_node.v_address_internal = free_mem_nodes->v_address_internal;

            // Adjust Memory Node (Split)
            free_mem_nodes->address += cur_node.size;
            free_mem_nodes->v_address_internal += cur_node.size;
            free_mem_nodes->size -= cur_node.size;
            // Add the Memory area to the Kernel Mapping
            uint64_t pages_to_change = cur_node.size / Page_Size;
            for (uint64_t i = 0; i < pages_to_change; ++i)
            {
                paging_edit_page_Sv39(&P_Kernel_table, cur_node.v_address_internal + (i * Page_Size), 0, Kernel_data_flags, true);
            }
            flushTLB_Cache();
            // Clean the Dirty Memory
            if (free_mem_nodes->mem_state == mem_flag_free_memory_dirty)
            {
                vera_utils_mem_set_64((uint64_t *)cur_node.v_address_internal, 0, cur_node.size);
            }

            // Copy Old Memory Map to the New Place
            if (*holder_node != NULL)
            {
#if INCLUDE_DEBUG
                vera_uart_print("Enter no NULL branch\n");
#endif
                // Get the Old Holder to copy the Memory Map to the new Memory Map Node Holder
                mem_node *node_holder_temp = *holder_node;
                // Loop Unrolling?
                vera_utils_mem_cpy64((uint64_t *)cur_node.v_address_internal, (uint64_t *)node_holder_temp->v_address_internal, node_holder_temp->size);

                // Old Holder gets set to dirty free memory
                node_holder_temp->mem_state = mem_flag_free_memory_dirty;

                pages_to_change = node_holder_temp->size / Page_Size;
                for (uint64_t i = 0; i < pages_to_change; ++i)
                {
                    paging_edit_page_Sv39(&P_Kernel_table, node_holder_temp->v_address_internal + (i * Page_Size), 0, Kernel_memory_map_flags, true);
                }
            }
            else
            {
#if INCLUDE_DEBUG
                vera_uart_print("Enter NULL branch\n");
#endif
                mem_node *nodes = (mem_node *)mem_nodes_holder->ptr_array;
                vera_utils_mem_cpy((uint8_t *)cur_node.v_address_internal, (uint8_t *)nodes, 64 * sizeof(mem_node));
            }

            // Adjust the Public Memory Map Array and search for the new Memory Node who holds this new Memory Map
            mem_nodes_holder->ptr_array = (void *)cur_node.v_address_internal;
            mem_node *memory_node = (mem_node *)mem_nodes_holder->ptr_array;
            // prepare for getting the Memory Node who holds the new Memory Map
            memory_node = mem_get_free_node(memory_node);
            *holder_node = memory_node;
            mem_nodes_holder->max_elements = cur_node.size / sizeof(mem_node);
            ++mem_nodes_holder->counter;
            // Set the Public Memory Node now the content of cur_node, who currently holds the Memory Map

            memory_node->address = cur_node.address;
            memory_node->mem_state = cur_node.mem_state;
            memory_node->process_ID = cur_node.process_ID;
            memory_node->size = cur_node.size;
            memory_node->v_address_internal = cur_node.v_address_internal;

#if INCLUDE_DEBUG
            vera_uart_print("Leave mem_make_mem_map\n");
#endif
            return;
        }
        // A currently existing Memory Node gets used, no new Node needed
        else if (space_in_pages == pages_min)
        {
#if INCLUDE_DEBUG
            vera_uart_printf("new_mem_map, no split:_ Pages_min: %il\n", pages_min);
#endif
            // Put it into the Kernel Mapping
            uint64_t pages_to_change = free_mem_nodes->size / Page_Size;
            // Get the new Holder
            mem_node *new_holder = mem_get_free_node((mem_node *)mem_nodes_holder->ptr_array);
            // Put the Informations into the new Holder
            new_holder->address = free_mem_nodes->address;
            new_holder->mem_state = free_mem_nodes->mem_state;
            new_holder->size = free_mem_nodes->size;
            new_holder->v_address_internal = free_mem_nodes->v_address_internal;
            // Invalid the old Free Mem Node.
            free_mem_nodes->address = 0;
            free_mem_nodes->mem_state = mem_flag_invalid;
            free_mem_nodes->size = 0;
            free_mem_nodes->v_address_internal = 0;
            if (mem_nodes_holder != &P_Free_Mem_nodes)
            {
                --P_Free_Mem_nodes.counter;
            }
            for (uint64_t i = 0; i < pages_to_change; ++i)
            {
                paging_edit_page_Sv39(&P_Kernel_table, new_holder->v_address_internal + (i * Page_Size), 0, Kernel_data_flags, true);
            }

            // Clean the Dirty Memory
            if (new_holder->mem_state == mem_flag_free_memory_dirty)
            {
                vera_utils_mem_set_64((uint64_t *)new_holder->v_address_internal, 0, new_holder->size);
            }

            // Copy Old Memory Map to the New Memory Map
            if (*holder_node != NULL)
            {
#if INCLUDE_DEBUG
                vera_uart_print("Enter no NULL branch\n");
#endif
                // Get the Old Holder to copy the Memory Map to the new Memory Map Node Holder
                mem_node *temp = *holder_node;
                vera_utils_mem_cpy64((uint64_t *)new_holder->v_address_internal, (uint64_t *)temp->v_address_internal, temp->size);

                // Old Holder gets set as free Dirty Memory
                temp->mem_state = mem_flag_free_memory_dirty;
                pages_to_change = temp->size / Page_Size;
                for (uint64_t i = 0; i < pages_to_change; ++i)
                {
                    paging_edit_page_Sv39(&P_Kernel_table, temp->v_address_internal + (i * Page_Size), 0, Kernel_memory_map_flags, true);
                }
            }
            else
            {
#if INCLUDE_DEBUG
                vera_uart_print("Enter NULL branch\n");
#endif
                mem_node *nodes = (mem_node *)mem_nodes_holder->ptr_array;
                vera_utils_mem_cpy((uint8_t *)new_holder->v_address_internal, (uint8_t *)nodes, 64 * sizeof(mem_node));
            }

            mem_nodes_holder->ptr_array = (void *)new_holder->v_address_internal;
            new_holder->mem_state = mem_flag_reserved_memory_system_map;
            new_holder->process_ID = Kernel_Process_ID;

            *holder_node = new_holder;
            mem_nodes_holder->max_elements = new_holder->size / sizeof(mem_node);
#if INCLUDE_DEBUG
            vera_uart_print("Leave mem_make_mem_map\n");
#endif
            return;
        }
        --element;
        ++free_mem_nodes;
    }
// Wenn wir hier ankommen heißt es das wir keine neue Memory Map Node finden konnten
#if INCLUDE_DEBUG
    vera_uart_print("Panic: mem_make_mem_map\n");
#endif
    vera_err_boot_panic(VERA_ERR_NOMEM);
    return;
}

/*
Unused currently
*/
static void mem_count_memory_stats()
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter mem_count_memory_stats\n");
#endif

#if INCLUDE_DEBUG
    vera_uart_print("Leave mem_count_memory_stats\n");
#endif
}

/*
    Get the next Free Node in the Array
*/
static mem_node *mem_get_free_node(mem_node *memory_nodes)
{
    while (memory_nodes->size != 0)
    {
        ++memory_nodes;
    }
    return memory_nodes;
}
// ------

// Kernel Setup Helper Function

/*
Helper funktion für "mem_init_kernel", hier wird der Kernel sowie der gesammte Memory bereich gemapped, in Higherhalf

params:
- boot_info -> Die Boot info, wo alles weitere drin ist um alles zu pagen
- k_table -> Der Page Table des Kernels

return:
- K_OK: Alles okay
- VERA_ERR_INVAL: V_addresse ist nicht aligned zu Page (4096:0x1000)
- Status: Was paging_map_pages_Sv39 zurück gibt
*/
static vera_state mem_first_paging(kernel_boot_info *const boot_info, paging_table_container *const k_table)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter mem_first_paging\n");
#endif
    uint64_t k_base = boot_info->kernel_region.kernel_base_VA;

    // TEMP während Lily == Kernel      Info: areas sind physisch wo er im RAM liegt, VA wird dazu addiert bei virt_address
    k_base = 0;
    // Text
    paging_map_pages_Sv39(k_table, (phys_address)boot_info->kernel_region.text_start, (virt_address)(boot_info->kernel_region.text_start + k_base), vera_paging_get_leaf_pages_amount(boot_info->kernel_region.text_end - boot_info->kernel_region.text_start), Kernel_text_flags, true, false);
    // roData
    paging_map_pages_Sv39(k_table, (phys_address)boot_info->kernel_region.rodata_start, (virt_address)(boot_info->kernel_region.rodata_start + k_base), vera_paging_get_leaf_pages_amount(boot_info->kernel_region.rodata_end - boot_info->kernel_region.rodata_start), Kernel_roData_flags, true, false);
    // Data
    paging_map_pages_Sv39(k_table, (phys_address)boot_info->kernel_region.data_start, (virt_address)(boot_info->kernel_region.data_start + k_base), vera_paging_get_leaf_pages_amount(boot_info->kernel_region.data_end - boot_info->kernel_region.data_start), Kernel_data_flags, true, false);
    // sdata
    paging_map_pages_Sv39(k_table, (phys_address)boot_info->kernel_region.sdata_start, (virt_address)(boot_info->kernel_region.sdata_start + k_base), vera_paging_get_leaf_pages_amount(boot_info->kernel_region.sdata_end - boot_info->kernel_region.sdata_start), Kernel_data_flags, true, false);
    // bss
    paging_map_pages_Sv39(k_table, (phys_address)boot_info->kernel_region.bss_start, (virt_address)(boot_info->kernel_region.bss_start + k_base), vera_paging_get_leaf_pages_amount(boot_info->kernel_region.bss_end - boot_info->kernel_region.bss_start), Kernel_data_flags, true, false);
    // stack_guard
    paging_map_pages_Sv39(k_table, (phys_address)boot_info->kernel_region.stack_guard_start, (virt_address)(boot_info->kernel_region.stack_guard_start + k_base), vera_paging_get_leaf_pages_amount(boot_info->kernel_region.stack_guard_end - boot_info->kernel_region.stack_guard_start), paging_Valid | paging_Read | Kernel_memory_map_flags, true, false);
    // stack
    paging_map_pages_Sv39(k_table, (phys_address)boot_info->kernel_region.stack_start, (virt_address)(boot_info->kernel_region.stack_start + k_base), vera_paging_get_leaf_pages_amount(boot_info->kernel_region.stack_end - boot_info->kernel_region.stack_start), Kernel_data_flags, true, false);

    mem_node *memory_node = (mem_node *)boot_info->mem_nodes;
    // Wenn der erste Node nicht Aligned ist zu 4096 (4KiB) dann beendet es sich direkt. TODO Lily bei Lily Refactor
    uint64_t k_offset = VA_UPPER_HALF_START + memory_node->address;
    // Memory Map
    for (uint8_t i = 0; i < boot_info->mem_nodes_counter; ++i)
    {
        mem_node *cur_node = &memory_node[i];
        // Es muss sicher sein das die Liste geordnet ist
        if (cur_node->mem_state == mem_flag_invalid)
        {
            break;
        }
        else if (cur_node->mem_state == mem_flag_reserved_unknown)
        {
            k_offset += cur_node->size;
            continue;
        }
        else if (cur_node->mem_state == mem_flag_reserved_kernel)
        {
            k_offset += boot_info->kernel_region.kernel_end - boot_info->kernel_region.kernel_start;
            continue;
        }
        cur_node->v_address_internal = k_offset;
        // V Addresse ist nicht alligned, sollte normal nie passieren
        if (cur_node->v_address_internal % Page_Size)
        {
            return VERA_ERR_INVAL;
        }
        // Addresse nicht aligned
        if ((cur_node->address % Page_Size))
        {
            uint16_t offset = cur_node->address % Page_Size;
            cur_node->address += offset;
            cur_node->size -= offset;
            P_Losed_memory += offset;
        }
        if ((cur_node->address + cur_node->size) % Page_Size)
        {
            uint16_t offset = (cur_node->address + cur_node->size) % Page_Size;
            cur_node->size -= offset;
            P_Losed_memory += offset;
        }
        vera_state status;
        if (cur_node->mem_state == mem_flag_reserved_kernel_heap || cur_node->mem_state == mem_flag_reserved_kernel_info || cur_node->mem_state == mem_flag_reserved_dtb)
        {
            if (cur_node->mem_state == mem_flag_reserved_dtb)
            {
                status = paging_map_pages_Sv39(k_table, (phys_address)cur_node->address, (virt_address)cur_node->address, vera_paging_get_leaf_pages_amount(cur_node->size), Kernel_roData_flags, true, false);
            }
            else
            {
                status = paging_map_pages_Sv39(k_table, (phys_address)cur_node->address, (virt_address)cur_node->v_address_internal, vera_paging_get_leaf_pages_amount(cur_node->size), Kernel_heap_flags, true, false);
            }
            if (cur_node->mem_state == mem_flag_reserved_kernel_heap)
            {
                cur_node->mem_state = mem_flag_reserved_kernel_page_table;
            }
        }
        else
        {
            status = paging_map_pages_Sv39(k_table, (phys_address)cur_node->address, (virt_address)cur_node->v_address_internal, vera_paging_get_leaf_pages_amount(cur_node->size), Kernel_memory_map_flags, false, false);
        }
        if (VERA_FAILED(status))
        {
            return status;
        }
        k_offset += cur_node->size;
    }
#if INCLUDE_DEBUG
    vera_uart_print("Leave mem_first_paging\n");
#endif
    return VERA_OK;
}
