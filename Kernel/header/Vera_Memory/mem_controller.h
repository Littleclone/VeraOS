/* header/Vera_Memory/mem_controller.h */
#pragma once

#include "../Vera_Utils/utils.h"
#include "../kernel.h"

typedef enum {
    mem_flag_invalid,
    mem_flag_free_memory_dirty = 1,
    mem_flag_free_memory_clean = 2,
    // Reserved Kernel

    mem_flag_reserved_kernel = 3,
    mem_flag_reserved_kernel_heap = 4,
    mem_flag_reserved_kernel_page_table = 5,

    // Other Reserved Stuff
    mem_flag_reserved_unknown = 6,
    mem_flag_reserved_kernel_info = 7,       // The information for the Kernel to start are here
    mem_flag_reserved_dtb = 8,
    mem_flag_reserved_framebuffer,
    mem_flag_reserved_mmio,
    
    // Mem System
    mem_flag_reserved_memory_system_map,

    // Reserved Userland

    mem_flag_reserved_userland,
    mem_flag_reserved_userland_heap,

}mem_mem_map;

typedef struct {
    phys_address address;                   // the starting Physical address
    size_t size;                            // the size in bytes
    virt_address v_address_internal;        // the Starting Virtuell Address
    virt_address v_address_user;            // the starting virtuell address for userland
    I_PID process_ID;                       // the intern process ID who owns this Memory area (Combine with mem_state), default Kernel = 0
    mem_mem_map mem_state;      // the state of the memory-area
    bool is_lost;
}mem_node;


vera_state mem_init_kernel(kernel_boot_info* const boot_info, void* const dtb_header_ptr);

mem_node *k_mem_request_heap_page(const size_t requested_bytes);

vera_state k_mem_free_pages(const virt_address v_addresse, uint64_t area_size, const I_PID from_intern_process_ID, void* const process_page_table, const bool kernel_pages);

vera_state k_mem_page_new_driver_area(const phys_address p_address, const virt_address v_address, const uint64_t area_size);

vera_state k_mem_unpage_new_driver_area(const phys_address p_address, const virt_address v_address, const uint64_t area_size);
