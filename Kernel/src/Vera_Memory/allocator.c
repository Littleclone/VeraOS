/* src/Vera_Memory/allocator.c */
#include "../../header/Vera_Memory/allocator.h"
#include "../../header/Vera_UART/uart.h"
#include "../../header/Vera_Memory/mem_controller.h"


#define Heap_Size_Init 0x8000 // 32KiB
#define Heap_Area_Elements_Init 128
#define Heap_Block_Not_Used 0

typedef struct
{
    memory_address block_start; // Der Wert des Pointers, muss immer 8 Byte aligned beginnen
    size_t area_size;           // Die Größe des bereiches, muss immer 8 Byte aligned enden
} heap_block;


inline static heap_block* alloc_get_next_free_block(heap_block* blocks);

inline static void alloc_count_used_blocks(vera_utils_safe_array* block_holder);

static void alloc_expand_heap_blocks(vera_utils_safe_array* block_holder);


// State of the Allocator

/*
Erster eintrag zeigt immer auf den Bereich der die Heap Map hält und die restlichen Heap Map einträgen folgen

Hier werden alle Heap Blöcke gespeichert die vom Kernel in benutzung sind.
*/
static vera_utils_safe_array P_Kernel_Heap_Used_Blocks;

/*
Erster eintrag zeigt immer auf den Bereich der die Heap Map hält und die restlichen Heap Map einträgen folgen

Hier werden alle Heap Blöcke gespeichert die dem Kernel frei zur verfügung stehen.
*/
static vera_utils_safe_array P_Kernel_Heap_Free_Blocks;

/*
Sagt an ob der Kernel Allocator initialisiert ist.
*/
static bool P_Allocator_Initialised = false;

/*
Initialisiert den Kernel Allocator für den Kernel

note:
- Setzt "P_Allocator_Initialised" auf True, dies erlaubt erst wenn true ist, diesen zu nutzen.

return:
- VERA_OK -> Alles okay
- VERA_ERR_NULL_PTR -> Beim Initialiseren der Allocator Internals ist etwas fehlgeschlagen
*/
vera_state alloc_init_allocator()
{
#if INCLUDE_DEBUG
    vera_uart_print("\nInit Kernel Allokator\n");
#endif

    // Init Allocator Area for Used Blocks
    mem_node *heap_mem = k_mem_request_heap_page(sizeof(heap_block) * Heap_Area_Elements_Init);
    if (heap_mem == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
    P_Kernel_Heap_Used_Blocks.ptr_array = (void *)heap_mem->v_address_internal;
    P_Kernel_Heap_Used_Blocks.max_elements = heap_mem->size / sizeof(heap_block);



    // Init Allocator Area for Free Blocks
    heap_mem = k_mem_request_heap_page(sizeof(heap_block) * Heap_Area_Elements_Init);
    if (heap_mem == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
    P_Kernel_Heap_Free_Blocks.ptr_array = (void *)heap_mem->v_address_internal;
    P_Kernel_Heap_Free_Blocks.max_elements = heap_mem->size / sizeof(heap_block);

    // Create an Holder Block for the Init Heap Size
    heap_block *holder_block = (heap_block *)P_Kernel_Heap_Free_Blocks.ptr_array;
    holder_block->block_start = (memory_address)heap_mem->v_address_internal;
    holder_block->area_size = heap_mem->size;



    // Machen ein init Heap:
    heap_mem = k_mem_request_heap_page(Heap_Size_Init);
    if (heap_mem == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
    heap_block *init_heap = (holder_block + 1);
    init_heap->block_start = (memory_address)heap_mem->v_address_internal;
    init_heap->area_size = heap_mem->size;


    P_Kernel_Heap_Used_Blocks.counter = 1;
    P_Kernel_Heap_Free_Blocks.counter = 2;

    
    P_Allocator_Initialised = true;
    
    #if INCLUDE_DEBUG
    vera_uart_print("Finished Kernel Allokator\n");
    #endif
    return VERA_OK;
}

/*
Gibt ein Pointer zurück für ein Reservierten Bereich im Kernel Heap, um dinge zu speichern.

params:
- requested_bytes -> Die anzahl an bytes die der Kernel allokieren will.

return:
- (ptr) void -> Ein Pointer im Heap den der Kernel nutzen kann
- NULL -> Wenn nicht allokiert werden konnte

note:
- Reserviert "reuqest_bytes" + 8 für ein Overflow Guard und Markierung
*/
void *k_malloc(size_t request_bytes)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter k_malloc\n");
#endif
    size_t temp = (size_t)vera_utils_align(request_bytes, 8);
    temp += 8;
    if (!P_Allocator_Initialised || temp < request_bytes)
    {
        #if INCLUDE_DEBUG
        vera_uart_print("Leave k_malloc\n");
#endif
        return NULL;
    }

    // Ist 8 Byte aligned und hat 8 Bytes für den Guard Protect dabei
    request_bytes = temp;

    // Search for a Block that is not used and has enough Space for "request_bytes"

    heap_block *free_blocks = (heap_block *)P_Kernel_Heap_Free_Blocks.ptr_array;
    ++free_blocks;
    uint64_t free_max_elements = P_Kernel_Heap_Used_Blocks.max_elements;
    uint64_t free_block_counter = P_Kernel_Heap_Used_Blocks.counter;

    heap_block* blocks = (heap_block*)P_Kernel_Heap_Used_Blocks.ptr_array;
    ++blocks;
    uint64_t max_elements = P_Kernel_Heap_Used_Blocks.max_elements;
    uint64_t block_counter = P_Kernel_Heap_Used_Blocks.counter;

    // Schauen ob wir mehr Platz brauchen
    if ((free_block_counter + 2) >= free_max_elements)
    {
        // Neue Liste organisieren
        alloc_expand_heap_blocks(&P_Kernel_Heap_Free_Blocks);
        free_max_elements = P_Kernel_Heap_Free_Blocks.max_elements;
        free_block_counter = P_Kernel_Heap_Free_Blocks.counter;
        free_blocks = (heap_block *)P_Kernel_Heap_Free_Blocks.ptr_array;
        ++free_blocks;
    }
    if ((block_counter + 2) >= max_elements) {
        // Neue Liste organisieren
        alloc_expand_heap_blocks(&P_Kernel_Heap_Used_Blocks);
        max_elements = P_Kernel_Heap_Used_Blocks.max_elements;
        block_counter = P_Kernel_Heap_Used_Blocks.counter;
        blocks = (heap_block *)P_Kernel_Heap_Used_Blocks.ptr_array;
        ++blocks;
    }

    heap_block *target_block = NULL;

    for (uint64_t i = 0; i < free_block_counter; ++i)
    {
        heap_block *temp_block = &free_blocks[i];

        if (temp_block->area_size > request_bytes)
        {
            target_block = temp_block;
            break;
        }
        else if (temp_block->area_size == request_bytes)
        {
            blocks = alloc_get_next_free_block(blocks);
            blocks->area_size = temp_block->area_size;
            blocks->block_start = temp_block->block_start;
            temp_block->block_start = 0;
            temp_block->area_size = 0;

            // Change Data
            --P_Kernel_Heap_Free_Blocks.counter;
            ++P_Kernel_Heap_Used_Blocks.counter;

#if INCLUDE_DEBUG
            vera_uart_printf("Gives Pointer to kernel: %p\n", blocks->block_start);
#endif
            uint64_t *guard = (uint64_t *)(blocks->block_start + blocks->area_size - 8);
            *guard = 0xDEADC0DE;
            return (void *)blocks->block_start;
        }
    }

    if (target_block == NULL)
    {
        // Haben kein Target Block gefunden, müssen mehr speicherplatz allokieren.
        uint64_t new_bytes = request_bytes + 0x4000;
        if (new_bytes < request_bytes) {
            return NULL;
        }
        mem_node *temp_new_holder = k_mem_request_heap_page(new_bytes);

        target_block = alloc_get_next_free_block(free_blocks);
        target_block->area_size = temp_new_holder->size;
        target_block->block_start = temp_new_holder->v_address_internal;

        ++P_Kernel_Heap_Free_Blocks.counter;
    }

    // Wenn wir hier sind wissen wir das der bereich der gehalten wird größer ist als das was wir brauchen
    heap_block *requested_block = alloc_get_next_free_block(blocks);

    requested_block->block_start = target_block->block_start;
    requested_block->area_size = request_bytes;

    target_block->block_start += request_bytes;
    target_block->area_size -= request_bytes;

    ++P_Kernel_Heap_Used_Blocks.counter;
#if INCLUDE_DEBUG
    vera_uart_printf("Gives Pointer to kernel: %p\n", requested_block->block_start);
#endif
    uint64_t *guard = (uint64_t *)(requested_block->block_start + requested_block->area_size - 8);
    *guard = 0xDEADC0DE;
    return (void *)requested_block->block_start;
}

/*
Nimmt den Pointer der von "k_malloc" bereitgestellt wurde und stellt den bereich
in der Heap List als Free

params:
- (ptr) ptr -> Pointer der von k_malloc überreich wurde

panic:
- VERA_ERR -> Wenn Overflow Guard overwritten ist, ist derzeit ein Panic.
*/
void k_free(void *ptr)
{

#if INCLUDE_DEBUG
    vera_uart_print("Enter k_free\n");
#endif
    if (!P_Allocator_Initialised) {
        #if INCLUDE_DEBUG
        vera_uart_print("Leave k_free\n");
        #endif
        return;
    }
    heap_block *free_blocks = (heap_block *)P_Kernel_Heap_Free_Blocks.ptr_array;
    ++free_blocks;
    uint64_t free_max_elements = P_Kernel_Heap_Used_Blocks.max_elements;
    uint64_t free_block_counter = P_Kernel_Heap_Used_Blocks.counter;

    // Schauen ob wir mehr Platz brauchen
    if ((free_block_counter + 2) >= free_max_elements)
    {
        // Neue Liste organisieren
        alloc_expand_heap_blocks(&P_Kernel_Heap_Free_Blocks);
        free_max_elements = P_Kernel_Heap_Free_Blocks.max_elements;
        free_block_counter = P_Kernel_Heap_Free_Blocks.counter;
        free_blocks = (heap_block *)P_Kernel_Heap_Free_Blocks.ptr_array;
        ++free_blocks;
    }
    uint64_t given_ptr = (uint64_t)ptr;

    // The blocks beginning
    heap_block *blocks = (heap_block *)P_Kernel_Heap_Used_Blocks.ptr_array;
    ++blocks;

    // Binary tree? TODO?

    // Search for the Block we want to free.
    heap_block* target_block = NULL;
    for (uint64_t i = 0; i < P_Kernel_Heap_Used_Blocks.counter; ++i) {
        if (blocks->block_start == given_ptr) {
            target_block = blocks;
        }
        ++blocks;
    }

    // Schauen ob wir den Target gefunden haben
    if (target_block == NULL) {
        return;
    }

    // Free'n Target und überprüfe Guard
    heap_block temp_holder;
    temp_holder.area_size = target_block->area_size;
    temp_holder.block_start = target_block->block_start;
    uint64_t* guard = (uint64_t*)(temp_holder.block_start + temp_holder.area_size - 8);
    if (*guard != 0xDEADC0DE) {
        vera_uart_print("Guard Overwritten!\n");
        // Temp
        vera_err_boot_panic(VERA_ERR);
    }

    target_block->area_size = 0;
    target_block->block_start = 0;
    --P_Kernel_Heap_Used_Blocks.counter;


    // --- Mergen ---
    target_block = alloc_get_next_free_block(free_blocks);
    target_block->block_start = temp_holder.block_start;
    target_block->area_size = temp_holder.area_size;
    ++P_Kernel_Heap_Free_Blocks.counter;
    
    uint64_t address_border = temp_holder.block_start + temp_holder.area_size;

    // Und sicherheit für kein Out of Bounds read/write
    uint64_t itteration_counter = 0;

    while (free_block_counter && itteration_counter >= free_max_elements) {
        heap_block* block = &free_blocks[itteration_counter];
        ++itteration_counter;
        if (block->area_size == Heap_Block_Not_Used) {
            continue;
        }
        --free_block_counter;
        uint64_t block_address_border = block->block_start + block->area_size;

        // Passt es zu unserer Node?
        if (temp_holder.block_start == block_address_border) {
            // Falls unser block wo startet wo ein andere freier endet.
            target_block->block_start = block->block_start;
            target_block->area_size += block->area_size;
            block->area_size = Heap_Block_Not_Used;
            block->block_start = Heap_Block_Not_Used;
            --P_Kernel_Heap_Free_Blocks.counter;
        }
        else if (address_border == block->block_start) {
            // Falls unser bereich wo endet wo ein anderer freier startet.
            target_block->area_size += block->area_size;
            block->area_size = Heap_Block_Not_Used;
            block->block_start = Heap_Block_Not_Used;
            --P_Kernel_Heap_Free_Blocks.counter;
        }
    }

#if INCLUDE_DEBUG
    vera_uart_print("Leave k_free\n");
#endif
}

/*
Gets the next Free usable Block in the List

return:
- (ptr) block free to use
*/
inline static heap_block* alloc_get_next_free_block(heap_block* blocks) {
    if (blocks == NULL) {
        return NULL;
    }
    while (blocks->area_size != Heap_Block_Not_Used) {
        ++blocks;
    }
    return blocks;
}

/*
Count the Blocks used in the Heap_Blocks Array until "block_flag_not_used"

pre:
- sort the blocks for better accuracy
*/
inline static void alloc_count_used_blocks(vera_utils_safe_array* block_holder)
{
    if (block_holder == NULL || block_holder->ptr_array == NULL) {
        return;
    }
    uint64_t counter = 0;
    heap_block* blocks = (heap_block*)block_holder->ptr_array;

    while (blocks->area_size != Heap_Block_Not_Used)
    {
        ++counter;
        ++blocks;
    }
    block_holder->counter = counter;
}

/*
Erweitert die Heap Block Area Map um 2048 Bytes und kopiert die alten
einträge rüber und setzt die Pointer refferenz neu sowie den Block Holder
*/
static void alloc_expand_heap_blocks(vera_utils_safe_array* block_holder)
{
    if (block_holder == NULL || block_holder->ptr_array == NULL) {
        return;
    }

#if INCLUDE_DEBUG
    vera_uart_print("Expand Memory Block Holder\n");
#endif

    uint64_t bytes_request = (block_holder->max_elements * sizeof(heap_block)) + 0x800;

    mem_node *new_Block = k_mem_request_heap_page(bytes_request);
    if (new_Block == NULL)
    {
        vera_err_boot_panic(VERA_ERR_NOMEM);
    }

    // Copy Old Heap_Blocks to new Heap_Blocks
    vera_utils_mem_cpy64((uint64_t *)new_Block->v_address_internal, (uint64_t *)block_holder->ptr_array, (block_holder->max_elements * sizeof(heap_block)));

    // Safe the Old Area to free it soon

    heap_block *holder = (heap_block *)block_holder->ptr_array;
    heap_block old_holder;
    old_holder.block_start = holder->block_start;
    old_holder.area_size = holder->area_size;

    // Make the Gloabl P_Kernel_Blocks
    block_holder->ptr_array = (void*)new_Block->v_address_internal;
    block_holder->max_elements = new_Block->size / sizeof(heap_block);

    // Make the first Element to the Block Holder
    holder = (heap_block *)block_holder->ptr_array;
    holder->area_size = new_Block->size;
    holder->block_start = (memory_address)new_Block->v_address_internal;

    // Free the Old Block_Holder space
    vera_state state = k_mem_free_pages((virt_address)old_holder.block_start, old_holder.area_size, Kernel_Process_ID, NULL, true);
    if (VERA_FAILED(state))
    {
        vera_err_boot_panic(state);
    }
// Finnished
#if INCLUDE_DEBUG
    vera_uart_print("Leave Expand Memory Block\n");
#endif
}