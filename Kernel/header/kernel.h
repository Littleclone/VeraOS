/* header/kernel.h */
#pragma once

#include <stdint.h>

typedef struct {
    // Kernel
    uint64_t kernel_start;
    uint64_t kernel_end;
    // Text
    uint64_t text_start;
    uint64_t text_end;
    // roData
    uint64_t rodata_start;
    uint64_t rodata_end;
    // Data
    uint64_t data_start;
    uint64_t data_end;
    // sData
    uint64_t sdata_start;
    uint64_t sdata_end;
    // bss
    uint64_t bss_start;
    uint64_t bss_end;
    // Stack Guard
    uint64_t stack_guard_start;
    uint64_t stack_guard_end;
    // Stack
    uint64_t stack_start;
    uint64_t stack_end;
    // Base VA
    uint64_t kernel_base_VA;
}kernel_area;


typedef struct {
    char magic[10];         // Muss immer "LILY_BOOT\0" sein
    void* mem_nodes;        // Memory Nodes, Lily and Kernel uses the Same Struct (More or less)
    uint8_t* temp_heap;     // pointer to the region where the Temp Heap for the Page Table is stored
    void* dtb;
    kernel_area kernel_region;
    uint64_t temp_heap_size;
    uint64_t general_memory;    // How many Memory is on the system for us in the DTB
    uint16_t mem_nodes_counter; // The Counter for mem_nodes
}kernel_boot_info;